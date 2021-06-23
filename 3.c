// brak zagłodzeń
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

int parseParameters(int argc, char **argv);

volatile unsigned int readerCount, writerCount; // Liczby wszystkich czytelników i pisarzy.
volatile unsigned int reading; // Liczba aktualnie czytających czytelników.
volatile unsigned int writing; // Liczba aktualnie piszących pisarzy.
volatile unsigned int waitingReaders; // Liczba czytelników czekających w kolejce do wejścia.
volatile unsigned int waitingWriters; // Liczba pisarzy czekających w kolejce do wejścia.

pthread_mutex_t mutex; // Semafor binarny zapewniający dostęp do zmiennych poprzez procedurę monitora tylko jednemu wątkowi w danym momencie.
pthread_cond_t canRead; // Zmienna warunkowa, na której czytelnicy czekają, aż będą mogli czytać
pthread_cond_t canWrite; // Zmienna warunkowa, na której pisarze czekają, aż będą mogli pisać.

#define printInfo() printf("ReaderQ: %i WriterQ: %i [in: R: %i W: %i]\n", readerCount - reading, writerCount - writing, reading, writing) // Makro wypisujące komunikat z liczbami czytelników i pisarzy czekających oraz będących w czytelni.
#define randomTime() ((rand() & ((1 << 16) - 1)) + 10000) // <10000, 10000 + 2^16 - 1>

// Procedury monitora.
void startReading() // Rozpoczęcie czytania przez czytelnika.
{
    pthread_mutex_lock(&mutex); // Czytelnik zajmuje mutex lub czeka na jego zwolnienie.
    if (waitingWriters > 0 || writing > 0) // Jeżeli którykolwiek pisarz czeka lub pisze
    {
        ++waitingReaders; // Zwiększamy o 1 liczbę czekających czytelników.
        pthread_cond_wait(&canRead, &mutex); // Czytelnik zwalnia mutex i zaczyna czekać, aż będzie mógł czytać.
        // Czytelnik przestaje czekać i z powrotem zajmuje mutex.
        // Liczbę czytelników czekających zmniejsza i piszących zwiększa pisarz kończący pisanie w procedurze "stopWriting".
    }
    else // Jeżeli żaden pisarz nie czeka i nie pisze
    {
        ++reading; // Zwiększamy o 1 liczbę czytających czytelników.
        printInfo(); // Wypisujemy komunikat.
    }
    pthread_mutex_unlock(&mutex); // Czytelnik zwalnia mutex.
}
void stopReading() // Zakończenie czytania przez czytelnika.
{
    pthread_mutex_lock(&mutex); // Czytelnik zajmuje mutex lub czeka na jego zwolnienie.
    --reading; // Zmniejszamy o 1 liczbę czytających czytelników.
    if (reading == 0) // Jeżeli żaden czytelnik już nie czyta
        pthread_cond_signal(&canWrite); // Budzimy dokładnie 1 czekającego pisarza lub nic nie robimy, jeżeli żaden nie czeka.
    pthread_mutex_unlock(&mutex); // Czytelnik zwalnia mutex.
}
void startWriting() // Rozpoczęcie pisania przez pisarza.
{
    pthread_mutex_lock(&mutex); // Pisarz zajmuje mutex lub czeka na jego zwolnienie.
    if (reading > 0 || writing > 0) // Jeżeli którykolwiek czytelnik czyta lub którykolwiek inny pisarz pisze
    {
        ++waitingWriters; // Zwiększamy o 1 liczbę czekających pisarzy.
        pthread_cond_wait(&canWrite, &mutex); // Pisarz zwalnia mutex i zaczyna czekać, aż będzie mógł pisać.
        // Pisarz przestaje czekać i z powrotem zajmuje mutex.
        --waitingWriters; // Zmniejszamy o 1 liczbę czekających pisarzy.
    }
    writing = 1; // Zaznaczamy, że dokładnie 1 pisarz pisze.
    printInfo(); // Wypisujemy komunikat.
    pthread_mutex_unlock(&mutex); // Pisarz zwalnia mutex.
}
void stopWriting() // Zakończenie pisania przez pisarza.
{
    pthread_mutex_lock(&mutex); // Pisarz zajmuje mutex lub czeka na jego zwolnienie.
    writing = 0; // Zaznaczamy, że już żaden pisarz nie pisze.
    if (waitingReaders == 0) // Jeżeli żaden czytelnik nie czeka
        pthread_cond_signal(&canWrite); // Budzimy dokładnie 1 pisarza, o ile którykolwiek czeka.
    else // Jeżeli którykolwiek czytelnik czeka
    {
        pthread_cond_broadcast(&canRead); // Budzimy wszystkich czytelników będących w kolejce do wejścia
        reading += waitingReaders; // Zwiększamy liczbę czytających czytelników o liczbę czekających.
        waitingReaders = 0; // Zmniejszamy do 0 liczbę czekających.
        // "reading" musimy zwiększyć już w tym miejscu, ponieważ pisarz może wykonać "startWriting" z warunkiem "reading == 0" zanim czytelnicy obudzeni "pthread_cond_broadcast(&canRead)" zwiększą zmienną "reading". W takiej sytuacji niezamierzenie w czytelni jednocześnie mogą znaleźć się czytelnicy i pisarz. Najprościej mówiąc, od razu po obudzeniu czytelnika przez pisarza kończącego pisanie, czytelnik ten powinien wejść do czytelni, czyli zakończyć swoje wywołanie procedury "startReading", ale z powodu sposobu działania zmiennych warunkowych w POSIX, czytelnik kończy swoje wywołanie "startReading" dopiero po zwolnieniu mutexa przez pisarza na końcu jego wywołania "stopWriting".
        printInfo(); // Wypisujemy komunikat.
    }
    pthread_mutex_unlock(&mutex); // Pisarz zwalnia mutex.
}

void* reader(void *a) // Funkcja wykonywana przez wątek czytelnika.
{
    srand(time(NULL));
    while (1)
    {
        startReading(); // Czytelnik czeka lub od razu zaczyna czytać.
        usleep(randomTime()); // Czytelnik czyta.
        stopReading(); // Czytelnik przestaje czytać.

        usleep(randomTime()); // Czytelnik ustawia się w kolejce do czytelni.
    }
    return NULL;
}

void* writer(void *a) // Funkcja wykonywana przez wątek pisarza.
{
    srand(time(NULL));
    while (1)
    {
        startWriting(); // Pisarz czeka lub od razu zaczyna pisać.
        usleep(randomTime()); // Pisarz pisze.
        stopWriting(); // Pisarz przestaje pisać.

        usleep(randomTime()); // Pisarz ustawia się w kolejce do czytelni.
    }
    return NULL;
}

/*
argumenty:
brak
dodatkowe opcje:
-R <liczba czytelników>
-W <liczba pisarzy>
sposób użycia:
program [-R <liczba czytelników>] [-W <liczba pisarzy>]
*/
int main(int argc, char **argv)
{
    readerCount = 10; // Zapisujemy domyślną liczbę czytelników. Jeżeli użytkownik poda własną liczbę, to zaktualizujemy ją w parseParameters.
    writerCount = 4; // Zapisujemy domyślną liczbę pisarzy.
    // Analizujemy parametry podane przy uruchomieniu programu. Jeżeli wystąpił błąd
    if (parseParameters(argc, argv) < 0)
    {
        printf("sposob uzycia: program [-R <liczba czytelnikow>] [-W <liczba pisarzy>]\n"); // Wypisujemy prawidłowy sposób użycia programu.
        return -1; // Kończymy program.
    }
    // Zerujemy liczniki.
    writing = 0;
    reading = 0;
    waitingWriters = 0;
    waitingReaders = 0;
    pthread_mutex_init(&mutex, NULL); // Inicjujemy semafor binarny.
    // Inicjujemy zmienne warunkowe.
    pthread_cond_init(&canWrite, NULL);
    pthread_cond_init(&canRead, NULL);
    pthread_t writers[writerCount];
    pthread_t readers[readerCount];
    unsigned int i;
    for (i = 0; i < writerCount; ++i) // Rozpoczynamy wątki pisarzy.
        pthread_create(&writers[i], NULL, writer, NULL);
    for (i = 0; i < readerCount; ++i) // Rozpoczynamy wątki czytelników.
        pthread_create(&readers[i], NULL, reader, NULL);
    for (i = 0; i < writerCount; ++i) // Czekamy głównym wątkiem na zakończenie wątków pisarzy.
        pthread_join(writers[i], NULL);
    for (i = 0; i < readerCount; ++i) // Czekamy głównym wątkiem na zakończenie wątków czytelników.
        pthread_join(readers[i], NULL);
    return 0;
}

int parseParameters(int argc, char **argv) // Funkcja wczytująca ewentualne opcje programu: liczbę czytelników i pisarzy.
{
    int option;
    while ((option = getopt(argc, argv, ":R:W:")) != -1) // Umieszczamy ':' na początku __shortopts, aby móc rozróżniać między '?' (nieznaną opcją) i ':' (brakiem podania wartości dla opcji)
    {
        switch (option)
        {
        case 'R':
            if (sscanf(optarg, "%u", &readerCount) < 1) // Ciąg znaków optarg jest liczbą czytelników. Zamieniamy go na liczbę typu unsigned int. Jeżeli sscanf nie wypełnił poprawnie zmiennej interval, to podana wartość czasu ma niepoprawny format i
                return -1; // Zwracamy kod błędu.
            break;
        case 'W':
            if (sscanf(optarg, "%u", &writerCount) < 1) // Ciąg znaków optarg jest liczbą pisarzy.
                return -2; // Zwracamy kod błędu.
            break;
        case ':':
            printf("opcja wymaga podania wartosci\n"); // Jeżeli podano opcję -W lub -R, ale nie podano jej wartości, to wypisujemy komunikat
            return -3; // Zwracamy kod błędu.
            break;
        case '?':
            printf("nieznana opcja: %c\n", optopt); // Jeżeli podano opcję inną niż -R, -W
            return -4; // Zwracamy kod błędu.
            break;
        default:
            printf("blad"); // Jeżeli getopt zwróciło wartość inną niż powyższe, co nie powinno się nigdy zdarzyć
            return -5; // Zwracamy kod błędu.
            break;
        }
    }
    int remainingArguments = argc - optind; // Wyznaczamy liczbę argumentów, które nie są opcjami (powinno być dokładnie 0).
    if (remainingArguments != 0) // Jeżeli nie mamy dokładnie 0 argumentów
        return -6; // Zwracamy kod błędu.
    return 0; // Zwracamy kod poprawnego zakończenia.
}
