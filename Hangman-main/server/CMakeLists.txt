cmake_minimum_required(VERSION 3.10)

# Dodajemy plik wykonywalny "server" z właściwymi plikami źródłowymi
add_executable(server
    main.cpp
    Wisielec.cpp
    # ... jeśli masz jeszcze jakieś inne .cpp potrzebne
)

# Opcjonalnie ustawiamy standard (tu C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ewentualne biblioteki systemowe
# target_link_libraries(server PRIVATE pthread)
