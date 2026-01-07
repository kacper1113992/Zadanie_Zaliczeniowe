import tkinter as tk
from tkinter import ttk
from tkinter import messagebox
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import serial
import serial.tools.list_ports
import random
import time

class TempControlApp:

    def __init__(self, root):
        self.root = root
        self.root.title("Panel Sterowania Temperaturą - STM32 SCADA")
        self.root.geometry("900x650")

        # --- ZMIENNE APLIKACJI ---
        self.ser = None
        self.is_connected = False
        self.target_temp = 25.0
        
        # --- ZMIENNE WYKRESU ---
        self.max_points = 100         # Ile punktów widocznych na wykresie (szerokość okna)
        self.time_counter = 0.0       # Licznik czasu (wirtualny czas osi X)
        self.update_interval = 200    # Odświeżanie co 200 ms
        
        # Listy na dane (bufor)
        self.time_data = []
        self.temp_data = []
        self.setpoint_data = []

        # --- BUDOWA INTERFEJSU ---
        self.create_connection_panel()
        self.create_dashboard()
        self.create_controls()
        self.create_graph()

        # Uruchomienie pętli głównej
        self.update_loop()

    def create_connection_panel(self):
        # Ramka połączenia
        frame = ttk.LabelFrame(self.root, text="Konfiguracja Połączenia")
        frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(frame, text="Port COM:").pack(side="left", padx=5)

        # Wyszukiwanie dostępnych portów
        ports = [p.device for p in serial.tools.list_ports.comports()]
        ports.append("Symulacja") # Opcja testowa bez sprzętu
        
        self.port_combo = ttk.Combobox(frame, values=ports, width=15)
        if ports:
            self.port_combo.set(ports[-1]) # Wybierz ostatni lub Symulację
        self.port_combo.pack(side="left", padx=5, pady=5)
        
        self.btn_connect = ttk.Button(frame, text="Połącz", command=self.toggle_connection)
        self.btn_connect.pack(side="left", padx=10)
        
        self.lbl_status = ttk.Label(frame, text="Rozłączono", foreground="red")
        self.lbl_status.pack(side="left", padx=10)

    def create_dashboard(self):
        # Panel z dużymi cyframi
        frame = ttk.Frame(self.root)
        frame.pack(fill="x", padx=10, pady=10)

        self.lbl_current = self.create_value_box(frame, "Aktualna Temp.", "--.- °C", "#d32f2f") # Czerwony
        
        # TU ZMIANA: Dodajemy flagę add_indicator=True dla środkowego pola
        self.lbl_target = self.create_value_box(frame, "Zadana Temp.", f"{self.target_temp:.1f} °C", "#388e3c", add_indicator=True) # Zielony
        
        self.lbl_error = self.create_value_box(frame, "Uchyb Regulacji", "0.00", "#1976d2") # Niebieski

    def create_value_box(self, parent, title, value, color, add_indicator=False):
        # Pomocnicza funkcja do tworzenia kafelków z danymi
        box = ttk.Frame(parent, borderwidth=2, relief="groove")
        box.pack(side="left", expand=True, fill="both", padx=5)
        
        # Kontener na tekst (żeby był po lewej/środku)
        text_frame = ttk.Frame(box)
        text_frame.pack(side="left", expand=True, fill="both")

        ttk.Label(text_frame, text=title, font=("Helvetica", 10, "bold")).pack(pady=5)
        lbl = ttk.Label(text_frame, text=value, font=("Helvetica", 24, "bold"), foreground=color)
        lbl.pack(pady=5)

        # --- NOWOŚĆ: Kontrolka Stanu ---
        if add_indicator:
            # Tworzymy płótno (Canvas) na kółko
            self.indicator_canvas = tk.Canvas(box, width=40, height=40, highlightthickness=0)
            self.indicator_canvas.pack(side="right", padx=15)
            
            # Rysujemy kółko (x1, y1, x2, y2). Kolor domyślny: szary (nieaktywny)
            # Tag 'status_light' pozwoli nam łatwo zmieniać kolor później
            self.indicator_id = self.indicator_canvas.create_oval(5, 5, 35, 35, fill="#cccccc", outline="#999999")
            
        return lbl

    def create_controls(self):
        # Panel sterowania
        frame = ttk.LabelFrame(self.root, text="Zadawanie Temperatury")
        frame.pack(fill="x", padx=10, pady=5)

        # Kontener centrujący
        inner = ttk.Frame(frame)
        inner.pack(pady=10)

        ttk.Button(inner, text="- 0.5 °C", command=lambda: self.change_setpoint(-0.5)).pack(side="left", padx=10)
        ttk.Button(inner, text="- 0.1 °C", command=lambda: self.change_setpoint(-0.1)).pack(side="left", padx=2)
        
        self.entry_setpoint = ttk.Entry(inner, width=10, font=("Helvetica", 12), justify="center")
        self.entry_setpoint.insert(0, str(self.target_temp))
        self.entry_setpoint.pack(side="left", padx=15)
        
        ttk.Button(inner, text="Ustaw", command=self.set_manual_setpoint).pack(side="left", padx=5)
        
        ttk.Button(inner, text="+ 0.1 °C", command=lambda: self.change_setpoint(0.1)).pack(side="left", padx=2)
        ttk.Button(inner, text="+ 0.5 °C", command=lambda: self.change_setpoint(0.5)).pack(side="left", padx=10)

    def create_graph(self):
        # Konfiguracja Matplotlib
        self.fig, self.ax = plt.subplots(figsize=(5, 4), dpi=100)
        self.fig.subplots_adjust(bottom=0.15, left=0.1, right=0.95, top=0.90) # Marginesy
        
        self.ax.set_title("Przebieg procesu regulacji")
        self.ax.set_xlabel("Czas [s]")
        self.ax.set_ylabel("Temperatura [°C]")
        self.ax.grid(True, linestyle='--', alpha=0.7)
        
        # Linie wykresu
        self.line_temp, = self.ax.plot([], [], 'r-', linewidth=2, label='Aktualna')
        self.line_set, = self.ax.plot([], [], 'g--', linewidth=2, label='Zadana')
        self.ax.legend(loc='upper left')

        # Integracja z Tkinter
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.root)
        self.canvas.get_tk_widget().pack(fill="both", expand=True, padx=10, pady=10)

    def toggle_connection(self):
        port = self.port_combo.get()
        if self.btn_connect["text"] == "Połącz":
            # --- ŁĄCZENIE ---
            if port == "Symulacja":
                self.is_connected = True
                self.lbl_status.config(text="Tryb Symulacji", foreground="orange")
            else:
                try:
                    self.ser = serial.Serial(port, 115200, timeout=0.1)
                    self.is_connected = True
                    self.lbl_status.config(text=f"Połączono: {port}", foreground="green")
                except Exception as e:
                    messagebox.showerror("Błąd", f"Nie można otworzyć portu {port}\n{e}")
                    return
            
            self.btn_connect["text"] = "Rozłącz"
            self.port_combo.config(state="disabled")
            
            # Reset wykresu przy nowym połączeniu
            self.time_data = []
            self.temp_data = []
            self.setpoint_data = []
            self.time_counter = 0.0
            
        else:
            # --- ROZŁĄCZANIE ---
            if self.ser and self.ser.is_open:
                self.ser.close()
            
            self.is_connected = False
            self.btn_connect["text"] = "Połącz"
            self.lbl_status.config(text="Rozłączono", foreground="red")
            self.port_combo.config(state="normal")
            self.lbl_current.config(text="--.- °C")

            # Reset kontrolki na szary
            if hasattr(self, 'indicator_canvas'):
                self.indicator_canvas.itemconfig(self.indicator_id, fill="#cccccc", outline="#999999")


    def change_setpoint(self, delta):
        self.target_temp = round(self.target_temp + delta, 2)
        self.update_setpoint_ui()
        self.send_command_to_stm()

    def set_manual_setpoint(self):
        try:
            val = float(self.entry_setpoint.get().replace(',', '.'))
            self.target_temp = val
            self.update_setpoint_ui()
            self.send_command_to_stm()
        except ValueError:
            messagebox.showwarning("Błąd", "Wpisz poprawną liczbę!")

    def update_setpoint_ui(self):
        self.lbl_target.config(text=f"{self.target_temp:.1f} °C")
        self.entry_setpoint.delete(0, tk.END)
        self.entry_setpoint.insert(0, str(self.target_temp))

    def send_command_to_stm(self):
        # Format komendy: "SET:25.5\n"
        cmd = f"SET:{self.target_temp:.1f}\n"
        if self.is_connected and self.ser and self.ser.is_open:
            try:
                self.ser.write(cmd.encode('utf-8'))
                print(f"TX: {cmd.strip()}")
            except Exception as e:
                print(f"Błąd wysyłania: {e}")

    def update_loop(self):
        if self.is_connected:
            current_temp = 0.0
            
            # --- 1. ODCZYT DANYCH ---
            if self.ser and self.ser.is_open:
                try:
                    # Czytamy wszystko
                    lines = self.ser.read_all().decode('utf-8').split('\n')
                    if len(lines) > 1:
                        raw_line = lines[-2].strip() # Bierzemy ostatnią pełną linię
                        
                        if raw_line and ";" in raw_line: # Sprawdzamy czy jest średnik!
                            parts = raw_line.split(";")
                            
                            # Pobieramy TEMP AKTUALNĄ
                            current_temp = float(parts[0])
                            
                            # Pobieramy TEMP ZADANĄ (Synchronizacja z przyciskami STM32!)
                            stm_target = float(parts[1])
                            
                            # Aktualizujemy target w GUI tylko jeśli różni się od naszego
                            if abs(self.target_temp - stm_target) > 0.01:
                                self.target_temp = stm_target
                                self.update_setpoint_ui()

                        elif raw_line: # Stara kompatybilność
                            current_temp = float(raw_line)
                            
                except Exception as e:
                    print(f"Błąd RX: {e}")
                    current_temp = self.temp_data[-1] if self.temp_data else 20.0
            else:
                # SYMULACJA
                last = self.temp_data[-1] if self.temp_data else 20.0
                diff = self.target_temp - last
                current_temp = last + (diff * 0.05) + random.uniform(-0.05, 0.05)

            # --- LOGIKA KONTROLKI STANU (LAMPKI) ---
            # Histereza przyjęta jako 0.5 stopnia (zgodnie z kodem STM32)
            color_fill = "#cccccc" # Szary domyślnie
            color_outline = "#999999"

            if current_temp < self.target_temp:
                # GRZANIE -> Czerwony
                color_fill = "#d32f2f" # Czerwony
                color_outline = "#b71c1c"
            elif current_temp > self.target_temp + 0.5:
                # CHŁODZENIE -> Niebieski
                color_fill = "#1976d2" # Niebieski
                color_outline = "#0d47a1"
            else:
                # OK (MARTWA STREFA) -> Zielony
                color_fill = "#388e3c" # Zielony
                color_outline = "#1b5e20"
            
            # Aktualizacja koloru kółka
            if hasattr(self, 'indicator_canvas'):
                 self.indicator_canvas.itemconfig(self.indicator_id, fill=color_fill, outline=color_outline)

            # --- 2. AKTUALIZACJA DANYCH WYKRESU ---
            self.time_counter += (self.update_interval / 1000.0)
            
            self.time_data.append(self.time_counter)
            self.temp_data.append(current_temp)
            self.setpoint_data.append(self.target_temp)

            if len(self.time_data) > self.max_points:
                self.time_data.pop(0)
                self.temp_data.pop(0)
                self.setpoint_data.pop(0)

            # --- 3. ODŚWIEŻANIE UI ---
            error = self.target_temp - current_temp
            self.lbl_current.config(text=f"{current_temp:.2f} °C")
            self.lbl_error.config(text=f"{error:.2f}")

            # --- 4. ODŚWIEŻANIE WYKRESU ---
            self.line_temp.set_data(self.time_data, self.temp_data)
            self.line_set.set_data(self.time_data, self.setpoint_data)
            
            if self.time_data:
                self.ax.set_xlim(min(self.time_data), max(self.time_data))
                y_min = min(min(self.temp_data), min(self.setpoint_data)) - 2
                y_max = max(max(self.temp_data), max(self.setpoint_data)) + 2
                self.ax.set_ylim(y_min, y_max)

            self.canvas.draw()

        # Zaplanuj kolejne wykonanie funkcji
        self.root.after(self.update_interval, self.update_loop)

# --- START APLIKACJI ---
if __name__ == "__main__":
    root = tk.Tk()
    app = TempControlApp(root)
    root.protocol("WM_DELETE_WINDOW", root.quit)
    root.mainloop()