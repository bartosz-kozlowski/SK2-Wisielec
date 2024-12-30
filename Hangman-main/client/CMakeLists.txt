cmake_minimum_required(VERSION 3.10)

project(client LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Automatyczne generowanie plików MOC (klasy z Q_OBJECT)
set(CMAKE_AUTOMOC ON)
# Automatyczne wywoływanie UIC dla plików .ui
set(CMAKE_AUTOUIC ON)

# Szukamy Qt5 lub Qt6 (Widgets, Network)
# Używasz modułu "either_qt6_or_qt5"? Jeśli to Twój custom .cmake, kontynuuj tak samo:
find_package(either_qt6_or_qt5 NAMES Qt6 Qt5 COMPONENTS Widgets Network REQUIRED)
set(QtX Qt${either_qt6_or_qt5_VERSION_MAJOR})
find_package(${QtX} COMPONENTS Widgets Network REQUIRED)

# Wskazujemy pliki źródłowe klienta (kod + UI):
set(SRCS
    main.cpp
    mywidget.ui
    mywidget.cpp
    mywidget.h
    # ewentualnie inne pliki .cpp/.h
)

# Budujemy wykonywalny plik o nazwie "client"
add_executable(client ${SRCS})

# Linkujemy z odpowiednimi bibliotekami Qt
target_link_libraries(client PRIVATE
    ${QtX}::Widgets
    ${QtX}::Network
)

# Jeśli chcesz skopiować folder images do build/client,
# robisz to tak (analogicznie jak w poprzednim kodzie):
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/client/images)

# Kopiuje zawartość folderu images ze źródeł do build/client/images
file(COPY ${CMAKE_SOURCE_DIR}/images/
     DESTINATION ${CMAKE_BINARY_DIR}/client/images)
