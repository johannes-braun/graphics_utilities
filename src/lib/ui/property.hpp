#pragma once

template<typename T, typename = void> struct has_op_deref : std::false_type { };
template<typename T> struct has_op_deref<T, std::void_t<decltype(std::declval<T>().operator*())>> : std::true_type {};
template<typename R> constexpr bool has_op_deref_v = has_op_deref<R>::value;

template<typename T, typename = void> struct has_op_arr : std::false_type { };
template<typename T> struct has_op_arr<T, std::void_t<decltype(std::declval<T>().operator->())>> : std::true_type {};
template<typename R> constexpr bool has_op_arr_v = has_op_arr<R>::value;

template<typename T>
class property
{
public:
    using getter_type = std::function<T()>;
    using setter_type = std::function<void(const T&)>;

    property() : value() {};
    template<typename... Args, typename = decltype(T(std::declval<Args&&>()...))>
    property(Args&&... args) : value(std::forward<Args&&>(args)...) {}

    property(getter_type getter, setter_type setter) : get(getter), set(setter) {};

    operator const T&() const noexcept { return get(); }
    getter_type get = [this]() -> T { return value; };
    setter_type set = [this](const T& v) { value = v; };

    template<typename R, typename = std::enable_if_t<std::is_convertible_v<T, R>>>
    property& operator=(property<R>&& other)
    {
        set(other.get());
        return *this;
    }

    template<typename R, typename = std::enable_if_t<std::is_convertible_v<T, R>>>
    property& operator=(const property<R>& other)
    {
        set(other.get());
        return *this;
    }

    template<typename = std::void_t<std::enable_if_t<std::is_pointer_v<T> || has_op_deref_v<T>>>>
    decltype(value.operator*()) operator*() const {
        return get().operator*();
    }
    template<typename = std::void_t<std::enable_if_t<std::is_pointer_v<T> || has_op_arr_v<T>>>>
    decltype(std::declval<const T>().operator->()) operator->() const {
        return get().operator->();
    }
    template<typename = std::void_t<std::enable_if_t<std::is_pointer_v<T> || has_op_arr_v<T>>>>
    decltype(std::declval<T>().operator->()) operator->() {
        return get().operator->();
    }

    template<typename = std::void_t<std::enable_if_t<!std::is_pointer_v<T> && !has_op_deref_v<T>>>>
    T& operator*() {
        return get();
    }
    template<typename = std::void_t<std::enable_if_t<!std::is_pointer_v<T> && !has_op_deref_v<T>>>>
    const T& operator*() const {
        return get();
    }
    template<typename = std::void_t<std::enable_if_t<!std::is_pointer_v<T> && !has_op_deref_v<T>>>>
    T* operator->() {
        return &get();
    }
    template<typename = std::void_t<std::enable_if_t<!std::is_pointer_v<T> && !has_op_deref_v<T>>>>
    const T* operator->() const {
        return &get();
    }

    template<typename... Args, typename = decltype(std::declval<T>()(std::declval<Args>()...))>
    auto operator()(Args... args)
    {
        return value(args);
    }

    property& operator++() {
        set(get() + 1);
        return *this;
    }

    property operator++(int) {
        property tmp;
        tmp.value = this->value;
        tmp.get = this->get;
        tmp.set = this->set;
        set(get() + 1);
        return tmp;
    }

    T value;
};