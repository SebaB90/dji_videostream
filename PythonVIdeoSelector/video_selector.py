from multiprocessing import shared_memory
import struct
import atexit
import signal
import sys

SHM_NAME = "my_shm"
SIZE = 4  # 1 int32

shm = None  # riferimento globale per il cleanup


def cleanup(*_):
    """Chiude e rimuove la shared memory se esiste."""
    global shm
    if shm is not None:
        try:
            shm.close()
        except FileNotFoundError:
            pass
        except Exception as e:
            print(f"[Python] Errore in shm.close(): {e}")

        try:
            shm.unlink()
        except FileNotFoundError:
            pass
        except Exception as e:
            print(f"[Python] Errore in shm.unlink(): {e}")

        print("[Python] Shared memory chiusa e rimossa.")
        shm = None


def main():
    global shm

    # Crea la shared memory
    shm = shared_memory.SharedMemory(name=SHM_NAME, create=True, size=SIZE)
    print(f"[Python] Shared memory '{SHM_NAME}' creata (SIZE={SIZE} byte).")

    # Registra cleanup all'uscita normale
    atexit.register(cleanup)

    # Se arriva Ctrl+C o SIGTERM → esci in modo che atexit esegua cleanup()
    signal.signal(signal.SIGINT, lambda sig, frame: sys.exit(0))
    signal.signal(signal.SIGTERM, lambda sig, frame: sys.exit(0))

    while True:
        try:
            s = input("Inserisci un intero da scrivere in shared memory (0 per uscire): ")
        except EOFError:
            # es. chiusura stdin → esci pulito
            break

        try:
            value = int(s)
        except ValueError:
            print("Valore non valido, riprova.")
            continue

        # Scrivi l'intero (int32) in shared memory
        shm.buf[:4] = struct.pack("i", value)
        print(f"[Python] Scritto valore = {value} in shared memory '{SHM_NAME}'")

        # Se l'utente digita 0 → esci e lascia fare cleanup
        if value == 0:
            break


if __name__ == "__main__":
    main()