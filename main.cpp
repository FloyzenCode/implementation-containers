#include <iostream>
#include <utility>

class Int
{
private:
    int value;

public:
    static int moves;
    static int copies;
    static int constructed;

    Int() = delete;

    explicit Int(int a) : value(a)
    {
        constructed++;
    }

    ~Int()
    {
        constructed--;
    }

    Int(const Int &other)
    {
        value = other.value;
        copies++;
        constructed++;
    }

    Int(Int &&other) noexcept
    {
        value = other.value;
        other.value = 0;
        moves++;
        constructed++;
    }

    Int &operator=(const Int &other) = delete;

    Int &operator=(Int &&other) = delete;
};
template <typename T>
class MyVector
{
public:
    explicit MyVector(size_t max_capacity = 32768);

    ~MyVector();

    MyVector(const MyVector &) = delete;

    MyVector(MyVector &&other) noexcept;

    MyVector &operator=(const MyVector &) = delete;

    MyVector &operator=(MyVector &&other) noexcept;

    T &operator[](size_t ind);

    const T &operator[](size_t ind) const;

    void pop();

    void push_back(const T &element);

    template <typename... Args>
    void emplace_back(Args... args);

private:
    static const int PROT = PROT_WRITE | PROT_READ;
    static int DEFAULT_FLAGS;

    static size_t PAGE_SIZE;

    void clear();

    bool need_increasing();

    void increase_capacity();

    void fallback_allocate();

    char *raw_data;
    size_t pages_allocated;
    size_t size;
};

template <typename T>
MyVector<T>::MyVector(size_t max_capacity)
{
    char *initial_address = (char *)mmap(nullptr, (max_capacity * sizeof(T) / PAGE_SIZE + 1) * PAGE_SIZE, PROT,
                                         DEFAULT_FLAGS, -1, 0);
    if (initial_address == MAP_FAILED)
    {
        throw runtime_error(string("MAP_FAILED1 ") + strerror(errno));
    }
    munmap(initial_address, (max_capacity * sizeof(T) / PAGE_SIZE + 1) * PAGE_SIZE);
    raw_data = (char *)mmap(initial_address, PAGE_SIZE, PROT, DEFAULT_FLAGS | MAP_FIXED_NOREPLACE, -1, 0);
    if (raw_data == MAP_FAILED)
    {
        throw runtime_error(string("MAP_FAILED2 ") + strerror(errno));
    }
    pages_allocated = 1;
    size = 0;
}

template <typename T>
void MyVector<T>::clear()
{
    T *data = (T *)raw_data;
    for (size_t i = 0; i < size; i++)
    {
        data[i].~T();
    }
    if (raw_data)
    {
        munmap(raw_data, pages_allocated * PAGE_SIZE);
    }
    pages_allocated = 0;
    size = 0;
    raw_data = nullptr;
}

template <typename T>
void MyVector<T>::increase_capacity()
{
    char *new_mem = (char *)mmap(raw_data + pages_allocated * PAGE_SIZE, pages_allocated * PAGE_SIZE, PROT,
                                 DEFAULT_FLAGS | MAP_FIXED_NOREPLACE, -1, 0);
    if (new_mem == MAP_FAILED)
    {
        fallback_allocate();
        return;
    }
    pages_allocated *= 2;
}

template <typename T>
void MyVector<T>::fallback_allocate()
{
    // cerr << "fallback_allocate called " << pages_allocated << endl;
    T *data = (T *)raw_data;
    T *new_memory = (T *)mmap(nullptr, pages_allocated * 2 * PAGE_SIZE, PROT, DEFAULT_FLAGS, -1, 0);
    if (new_memory == MAP_FAILED)
    {
        throw runtime_error(string("MAP_FAILED in fallback_allocate ") + strerror(errno));
    }
    for (size_t i = 0; i < size; i++)
    {
        new (&new_memory[i]) T(std::move(data[i]));
        data[i].~T();
    }
    munmap(raw_data, pages_allocated * PAGE_SIZE);
    pages_allocated *= 2;
    raw_data = (char *)new_memory;
}

template <typename Key, typename Value>
class Map
{
private:
    struct Node
    {
        Key key;
        Value value;
        Node *left;
        Node *right;

        Node() : left(nullptr), right(nullptr) {}
        Node(Key k, Value v) : key(k), value(v), left(nullptr), right(nullptr) {}
    };

    Node *root_;

public:
    Map() : root_(nullptr) {}

    ~Map()
    {
        clear(root_);
    }

    void insert(Key key, Value value)
    {
        Node *parent = nullptr;
        Node *current = root_;

        // Поиск места для вставки
        while (current != nullptr)
        {
            parent = current;
            if (key < current->key)
            {
                current = current->left;
            }
            else if (current->key < key)
            {
                current = current->right;
            }
            else
            {
                // Ключ уже существует, просто заменяем значение
                current->value = value;
                return;
            }
        }

        // Создание нового узла
        Node *node = new Node(key, value);
        if (parent == nullptr)
        {
            // Это корень, дерево на данный момент пустое
            root_ = node;
        }
        else if (key < parent->key)
        {
            parent->left = node;
        }
        else
        {
            parent->right = node;
        }
    }

    bool contains(Key key) const
    {
        Node *current = root_;

        // Поиск узла с заданным ключом
        while (current != nullptr)
        {
            if (key < current->key)
            {
                current = current->left;
            }
            else if (current->key < key)
            {
                current = current->right;
            }
            else
            {
                // Ключ найден
                return true;
            }
        }

        // Ключ не найден
        return false;
    }

    Value &operator[](Key key)
    {
        Node *parent = nullptr;
        Node *current = root_;

        // Поиск места для вставки
        while (current != nullptr)
        {
            parent = current;
            if (key < current->key)
            {
                current = current->left;
            }
            else if (current->key < key)
            {
                current = current->right;
            }
            else
            {
                // Ключ уже существует, просто возвращаем значение
                return current->value;
            }
        }

        // Создание нового узла
        Node *node = new Node(key, Value());
        if (parent == nullptr)
        {
            // Это корень, дерево на данный момент пустое
            root_ = node;
        }
        else if (key < parent->key)
        {
            parent->left = node;
        }
        else
        {
            parent->right = node;
        }

        // Возвращаем ссылку на вновь созданное значение
        return node->value;
    }

    void clear()
    {
        clear(root_);
        root_ = nullptr;
    }

private:
    void clear(Node *node)
    {
        if (node != nullptr)
        {
            clear(node->left);
            clear(node->right);
            delete node;
        }
    }
};

// tuple
template <typename... Ts>
struct tuple
{
};

template <typename T, typename... Ts>
struct tuple<T, Ts...> : tuple<Ts...>
{
    tuple(T t, Ts... ts) : tuple<Ts...>(ts...), tail(t) {}

    T tail;
};

template <size_t, typename>
struct elem_type_holder;

template <typename T, typename... Ts>
struct elem_type_holder<0, tuple<T, Ts...>>
{
    using type = T;
};

template <size_t k, typename T, typename... Ts>
struct elem_type_holder<k, tuple<T, Ts...>>
{
    using type = typename elem_type_holder<k - 1, tuple<Ts...>>::type;
};

template <size_t k, typename... Ts>
auto get(const tuple<Ts...> &t) -> typename elem_type_holder<k, tuple<Ts...>>::type &
{
    using TupleType = tuple<Ts...>;
    using ElementType = typename elem_type_holder<k, TupleType>::type;

    TupleType &base = const_cast<TupleType &>(t);
    return static_cast<ElementType &>(base.tail);
}

template <typename T1, typename T2>
class my_pair
{
public:
    T1 first;
    T2 second;

    // Конструкторы
    my_pair() : first(T1()), second(T2()) {}
    my_pair(const T1 &t1, const T2 &t2) : first(t1), second(t2) {}
    template <typename U1, typename U2>
    my_pair(const my_pair<U1, U2> &p) : first(p.first), second(p.second) {}

    // Операторы сравнения
    bool operator==(const my_pair &rhs) const
    {
        return first == rhs.first && second == rhs.second;
    }
    bool operator!=(const my_pair &rhs) const
    {
        return !(*this == rhs);
    }
};

template <typename T>
class my_allocator
{
public:
    // Размер типа
    typedef size_t size_type;

    // Тип указателя
    typedef T *pointer;

    // Тип ссылки
    typedef T &reference;

    // Тип указателя на const объект
    typedef const T *const_pointer;

    // Тип ссылки на const объект
    typedef const T &const_reference;

    // Тип элемента аллокатора, которые является другим аллокатором
    template <typename U>
    struct rebind
    {
        typedef my_allocator<U> other;
    };

    // Конструктор по умолчанию
    my_allocator() {}

    // Конструктор копирования
    my_allocator(const my_allocator &) {}

    // Конструктор приведения типов
    template <typename U>
    my_allocator(const my_allocator<U> &) {}

    // Деструктор
    ~my_allocator() {}

    // Аллокация памяти
    pointer allocate(size_type n, const void *hint = nullptr)
    {
        pointer p = static_cast<pointer>(::operator new(n * sizeof(T)));
        if (!p)
        {
            throw std::bad_alloc();
        }
        return p;
    }

    // Освобождение памяти
    void deallocate(pointer p, size_type n)
    {
        ::operator delete(p);
    }

    // Возвращает максимальное количество максимальных размеров, которые аллокатор может удерживать
    size_type max_size() const
    {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    // Конструирование объекта
    template <typename U, typename... Args>
    void construct(U *p, Args &&...args)
    {
        ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...);
    }

    // Уничтожение объекта
    template <typename U>
    void destroy(U *p)
    {
        p->~U();
    }
};

// TEST AND RUN FOR CHECK EXAMPLE

class TestImplementations
{
public:
    int RUN_VECTOR();
    int RUN_MAP();
    int RUN_TUPLE();
    int RUN_PAIR();
    int RUN_ALLOCATOR();
};

int TestImplementations::RUN_VECTOR()
{
    int initial_size = 100000000;
    // auto my_vec = MyVector<Int>(PAGE_SIZE / sizeof(Int));
    auto my_vec = MyVector<Int>(initial_size * 2);
    // auto my_vec = vector<Int>();
    // my_vec.reserve(PAGE_SIZE / sizeof(Int));
    //  my_vec.reserve(initial_size);
    for (size_t i = 0; i < initial_size; i++)
    {
        my_vec.emplace_back(i);
    }
    std::printf("copied: %d, moved: %d, left: %d\n", Int::copies, Int::moves, Int::constructed);
    return 0;
}

int TestImplementations::RUN_MAP()
{
    Map<std::string, int> my_map;
    my_map.insert("foo", 42);
    my_map["bar"] = 1337;
    std::cout << my_map["foo"] << std::endl;
    std::cout << std::boolalpha << my_map.contains("baz") << std::endl;
    my_map.clear();
    return 0;
}

int TestImplementations::RUN_TUPLE()
{
    tuple<double, int, const char *> t(3.14, 42, "hello");

    std::cout << get<0>(t) << std::endl;
    std::cout << get<1>(t) << std::endl;
    std::cout << get<2>(t) << std::endl;

    return 0;
}

int TestImplementations::RUN_PAIR()
{
    my_pair<int, std::string> p(42, "LMAO");
    std::cout << "\nPair example:\n";
    std::cout << "First element: " << p.first << std::endl;
    std::cout << "Second element: " << p.second << std::endl;
    return 0;
}

int TestImplementations::RUN_ALLOCATOR()
{
    const size_t arr_size = 5;
    my_allocator<int> alloc;

    // Использование аллокатора для выделения памяти
    int *arr = alloc.allocate(arr_size);

    for (size_t i = 0; i < arr_size; ++i)
    {
        alloc.construct(&arr[i], (i + 1) * 10);
    }

    std::cout << "\nAllocator example:\n";
    std::cout << "Allocated array: ";
    for (size_t i = 0; i < arr_size; ++i)
    {
        std::cout << arr[i] << ' ';
    }
    std::cout << std::endl;

    for (size_t i = 0; i < arr_size; ++i)
    {
        alloc.destroy(&arr[i]);
    }

    // Использование аллокатора для освобождения памяти
    alloc.deallocate(arr, arr_size);

    return 0;
}
