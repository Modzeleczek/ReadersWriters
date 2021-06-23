// zagłodzenie czytelników
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

int parseParameters(int argc, char **argv);

volatile unsigned int readerCount, writerCount; // Liczby wszystkich czytelników i pisarzy.
volatile unsigned int reading; // Liczba aktualnie czytających czytelników.
volatile unsigned int writing; // Liczba aktualnie piszących pisarzy.
volatile unsigned int writingOrWaiting; // Liczba pisarzy aktualnie czekających w kolejce do wejścia lub piszących. Piszący może być maksymalnie 1 w danym momencie.

pthread_mutex_t rMutex; // Semafor binarny zapewniający dostęp do sekcji wejściowej lub wyjściowej tylko jednemu czytelnikowi w danym momencie.
pthread_mutex_t wMutex; // Semafor binarny zapewniający dostęp do sekcji wejściowej lub wyjściowej tylko jednemu pisarzowi w danym momencie.
pthread_mutex_t readerLock; // Semafor binarny blokujący dostęp do sekcji wejściowej wszystkim czytelnikom lub zapewniający tylko jednemu w danym momencie.
pthread_mutex_t writerLock; // Semafor binarny blokujący dostęp do czytelni wszystkim pisarzom lub zapewniający tylko jednemu w danym momencie.

#define printInfo() printf("ReaderQ: %i WriterQ: %i [in: R: %i W: %i]\n", readerCount - reading, writerCount - writing, reading, writing) // Makro wypisujące komunikat z liczbami czytelników i pisarzy czekających oraz będących w czytelni.
#define randomTime() ((rand() & ((1 << 16) - 1)) + 10000) // <10000, 10000 + 2^16 - 1>

void* reader(void *a) // Funkcja wykonywana przez wątek czytelnika.
{
    srand(time(NULL));
    while (1)
    {
        // Sekcja wejściowa czytelnika.
        pthread_mutex_lock(&readerLock); // Czytelnik czeka na moment, kiedy żaden pisarz nie jest w czytelni ani nie czeka w kolejce do wejścia.
        pthread_mutex_lock(&rMutex); // Czytelnik czeka na zwolnienie sekcji wejściowej przez innego czytelnika lub wchodzi do niej, blokując dostęp innym czytelnikom.
        if (reading == 0) // Jeżeli bieżący czytelnik jest pierwszym wchodzącym po wyjściu pisarza
            pthread_mutex_lock(&writerLock); // Czytelnik zostaje zatrzymany, jeżeli aktualnie w czytelni jest pisarz. W przeciwnym przypadku blokujemy pisarzom dostęp do czytelni.
        ++reading; // Zwiększamy o 1 liczbę czytających czytelników.
        printInfo(); // Wypisujemy komunikat.
        pthread_mutex_unlock(&rMutex); // Czytelnik zwalnia sekcję wejściową innym czytelnikom.
        pthread_mutex_unlock(&readerLock); // Czytelnik wchodzi do czytelni.
        
        // Sekcja czytania czytelnika.
        usleep(randomTime()); // Czytelnik czyta.

        // Sekcja wyjściowa czytelnika.
        pthread_mutex_lock(&rMutex); // Czytelnik czeka na zwolnienie sekcji wyjściowej przez innego czytelnika lub wchodzi do niej, blokując dostęp innym czytelnikom.
        --reading; // Zmniejszamy o 1 liczbę czytających czytelników.
        if (reading == 0) // Jeżeli bieżący jest ostatnim czytelnikiem, który wychodzi z czytelni
            pthread_mutex_unlock(&writerLock); // Odblokowujemy pisarzom dostęp do czytelni.
        pthread_mutex_unlock(&rMutex); // Czytelnik zwalnia sekcję wyjściową innym czytelnikom.

        usleep(randomTime()); // Czytelnik ustawia się w kolejce do czytelni.
    }
    return NULL;
}

void* writer(void *a) // Funkcja wykonywana przez wątek pisarza.
{
    srand(time(NULL));
    while (1)
    {
        // Sekcja wejściowa pisarza.
        pthread_mutex_lock(&wMutex); // Pisarz czeka na zwolnienie sekcji wejściowej przez innego pisarza lub wchodzi do niej, blokując dostęp innym pisarzom.
        ++writingOrWaiting; // Zwiększamy o 1 liczbę pisarzy czekających w kolejce lub piszących.
        if (writingOrWaiting == 1) // Jeżeli bieżący pisarz jest pierwszym pisarzem czekającym w kolejce lub wchodzącym do czytelni po wyjściu czytelników
            pthread_mutex_lock(&readerLock); // Blokujemy czytelnikom możliwość wejścia do czytelni.
        pthread_mutex_unlock(&wMutex); // Pisarz zwalnia sekcję wejściową innym pisarzom.

        pthread_mutex_lock(&writerLock); // Pisarz wchodzi do czytelni lub zostaje zatrzymany, jeżeli są w niej czytelnicy lub inny pisarz.
        ++writing; // Zwiększamy o 1 liczbę piszących pisarzy.
        printInfo(); // Wypisujemy komunikat.

        // Sekcja pisania pisarza.
        usleep(randomTime()); // Pisarz pisze.

        --writing; // Zmniejszamy o 1 liczbę piszących pisarzy.
        pthread_mutex_unlock(&writerLock); // Pisarz wychodzi z czytelni.

        // Sekcja wyjściowa pisarza.
        pthread_mutex_lock(&wMutex); // Pisarz czeka na zwolnienie sekcji wyjściowej przez innego pisarza lub wchodzi do niej, blokując dostęp innym pisarzom.
        --writingOrWaiting; // Zmniejszamy o 1 liczbę pisarzy czekających w kolejce lub piszących.
        if (writingOrWaiting == 0) // Jeżeli bieżący jest ostatnim pisarzem, który wychodzi z czytelni
            pthread_mutex_unlock(&readerLock); // Odblokowujemy czytelnikom dostęp do czytelni.
        pthread_mutex_unlock(&wMutex); // Pisarz zwalnia sekcję wyjściową innym pisarzom.

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
    if (parseParameters(argc, argv) < 0) // Analizujemy parametry podane przy uruchomieniu programu. Jeżeli wystąpił błąd
    {
        printf("sposob uzycia: program [-R <liczba czytelnikow>] [-W <liczba pisarzy>]\n"); // Wypisujemy prawidłowy sposób użycia programu.
        return -1; // Kończymy program.
    }
    // Zerujemy liczniki.
    writing = 0;
    writingOrWaiting = 0;
    reading = 0;
    // Inicjujemy semafory binarne.
    pthread_mutex_init(&wMutex, NULL);
    pthread_mutex_init(&rMutex, NULL);
    pthread_mutex_init(&writerLock, NULL);
    pthread_mutex_init(&readerLock, NULL);
    pthread_t writers[writerCount];
    pthread_t readers[readerCount];
    unsigned int i;
    for (i = 0; i < readerCount; ++i) // Rozpoczynamy wątki czytelników.
        pthread_create(&readers[i], NULL, reader, NULL);
    sleep(2); // Czekamy 2 sekundy przed rozpoczęciem wątków pisarzy, aby opóźnić moment opanowania przez nich czytelni, po którym następuje głodzenie czytelników.
    for (i = 0; i < writerCount; ++i) // Rozpoczynamy wątki pisarzy.
        pthread_create(&writers[i], NULL, writer, NULL);
    for (i = 0; i < readerCount; ++i) // Czekamy głównym wątkiem na zakończenie wątków czytelników.
        pthread_join(readers[i], NULL);
    for (i = 0; i < writerCount; ++i) // Czekamy głównym wątkiem na zakończenie wątków pisarzy.
        pthread_join(writers[i], NULL);
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
