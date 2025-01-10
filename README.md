# Gra wisielec 
### Bartosz Kozłowski [155869]  Agnieszka Maleszka [155941]

**Uruchomienie:**
1. **Przejdź do katalogu projektu:**
   ```bash
   cd Hangman-main
   ```
2. **Utworzenie katalogu build:**
   ```bash
   mkdir build
   cd build
   ```
3. **Generowanie plików za pomocą Cmake i zbudowanie proejtku**
  ```bash
  cmake ..
  cmake --build .
  ```
4. **Uruchomienie serwera i klienta**
   * klient
     ```bash
     cd client
     ./client
     ```
   * serwer
     ```bash
     cd serwer
     ./serwer

**Wymagania:**

* g++ wersja 10.x.x
* c++ wersja 10.x.x

**Opis:**

Gracz łączy się z serwerem i wysyła swój unikalny nick. Jeśli wybrany nick jest już zajęty, serwer prosi o wybór innego.
Po zaakceptowaniu nicku, gracz trafia do menu, gdzie może dołączyć do gry — w danym momencie istnieje tylko jeden pokój, do którego wszyscy mogą dołączyć.
W pokoju gry widoczna jest lista aktywnych graczy, ich wisielców oraz dotychczas zdobyte punkty.
Za każdą poprawną literę gracz zdobywa punkt, a odgadywanie liter trwa, dopóki wisielec nie zostanie ukończony.
Gracze mogą dołączać do gry w trakcie jej trwania, ale muszą poczekać na zakończenie bieżącej rundy, zanim wezmą udział w kolejnej.
Gra rozpoczyna się automatycznie po dołączeniu przynajmniej dwóch graczy.
Hasło wyświetlane jest w postaci pustych linii (jedna linia na każdą literę w haśle), a gracze muszą odgadywać litery.
Jeśli gracz odgadnie literę, zostaje ona ujawniona w odpowiednim miejscu w haśle.
Podczas każdej tury wyświetlana jest kategoria, z której pochodzi hasło.
Gracze mają 1,5 minuty na odgadnięcie hasła. Jeśli jeden z graczy odgadnie hasło, pozostali gracze mają dodatkowe 20 sekund na udzielenie swojej odpowiedzi, chyba że pozostały czas jest krótszy. Tura kończy się, gdy upłynie czas, wszyscy gracze popełnią zbyt wiele błędów, lub wszyscy odgadną hasło.
Jeśli w trakcie tury w pokoju pozostanie tylko jeden aktywny gracz (np. z powodu rozłączenia innych graczy), tura trwa do końca. Po jej zakończeniu, gracz trafia do poczekalni.
Gracz, który jako pierwszy odgadnie całe hasło, otrzymuje dodatkową premię punktową. Wartość premii maleje dla kolejnych graczy, którzy odgadną hasło w ramach tej samej tury.
Jeżeli gracze odgadną hasło w tym samym momencie, wszyscy ci gracze zdobywają punkty.
