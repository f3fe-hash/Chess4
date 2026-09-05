import socket
import threading

from config import ENGINE_MOVE_TIME_MS


class UCIClient:
    def __init__(self, host, port, on_bestmove, on_status):
        self.host = host
        self.port = port

        self.on_bestmove = on_bestmove
        self.on_status = on_status

        self.socket = None
        self.reader = None

        self.connected = False
        self.receive_thread = None

        self.send_lock = threading.Lock()

    # --------------------------------------------------------
    # Connect
    # --------------------------------------------------------

    def connect(self):
        if self.connected:
            return True

        try:
            sock = socket.socket(
                socket.AF_INET,
                socket.SOCK_STREAM
            )

            sock.settimeout(5)

            sock.connect(
                (self.host, self.port)
            )

            sock.settimeout(None)

            reader = sock.makefile(
                "r",
                encoding="utf-8",
                newline="\n"
            )

            self.socket = sock
            self.reader = reader
            self.connected = True

            self.on_status(
                f"Connected to {self.host}:{self.port}"
            )

            self.receive_thread = threading.Thread(
                target=self.receive_loop,
                daemon=True
            )

            self.receive_thread.start()

            self.send("uci")

            return True

        except Exception as e:
            self.connected = False

            self.on_status(
                f"Connection failed: {e}"
            )

            return False

    # --------------------------------------------------------
    # Receive loop
    # --------------------------------------------------------

    def receive_loop(self):
        try:
            while self.connected:
                line = self.reader.readline()

                if not line:
                    break

                line = line.strip()

                if not line:
                    continue

                print(f"<< {line}")

                if line == "uciok":
                    self.send("isready")

                elif line == "readyok":
                    self.on_status("Engine ready.")

                elif line.startswith("bestmove"):
                    parts = line.split()

                    if len(parts) >= 2:
                        self.on_bestmove(parts[1])

        except Exception as e:
            if self.connected:
                self.on_status(
                    f"Network error: {e}"
                )

        finally:
            was_connected = self.connected

            self.connected = False

            self._close_socket()

            if was_connected:
                self.on_status("Disconnected.")

    # --------------------------------------------------------
    # Send command
    # --------------------------------------------------------

    def send(self, command):
        if not self.connected:
            return False

        try:
            print(f">> {command}")

            with self.send_lock:
                self.socket.sendall(
                    (command + "\n").encode("utf-8")
                )

            return True

        except Exception as e:
            self.on_status(
                f"Send error: {e}"
            )

            self.connected = False

            return False

    # --------------------------------------------------------
    # Position
    # --------------------------------------------------------

    def send_position(self, moves):
        command = "position startpos"

        if moves:
            command += " moves " + " ".join(moves)

        return self.send(command)

    # --------------------------------------------------------
    # Search
    # --------------------------------------------------------

    def go(self, movetime_ms=ENGINE_MOVE_TIME_MS):
        return self.send(
            f"go movetime {max(1, int(movetime_ms))}"
        )

    def stop(self):
        return self.send("stop")

    # --------------------------------------------------------
    # New game
    # --------------------------------------------------------

    def new_game(self):
        return self.send("ucinewgame")

    # --------------------------------------------------------
    # Close
    # --------------------------------------------------------

    def close(self):
        self.connected = False
        self._close_socket()

    # --------------------------------------------------------
    # Internal socket cleanup
    # --------------------------------------------------------

    def _close_socket(self):
        if self.socket is not None:
            try:
                self.socket.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass

            try:
                self.socket.close()
            except Exception:
                pass

            self.socket = None

        if self.reader is not None:
            try:
                self.reader.close()
            except Exception:
                pass

            self.reader = None