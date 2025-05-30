import requests
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime
import os
import streamlit as st

# Kalibrierungswerte für die Umrechnung in Watt/m² (aus C++ Code übernommen)
DARK_ADC_VALUE = 4095      # ADC-Wert bei Dunkelheit (ESP32: 0-4095)
BRIGHT_ADC_VALUE = 0       # ADC-Wert bei hellem Licht
DARK_IRRADIANCE = 0.0      # W/m² bei Dunkelheit
BRIGHT_IRRADIANCE = 1000.0 # W/m² bei hellem Sonnenlicht (ca. 1000 W/m²)

# Umrechnung des ADC-Werts in W/m²
def convert_to_irradiance(adc_value):
    if DARK_ADC_VALUE == BRIGHT_ADC_VALUE: # Division durch Null vermeiden
        return DARK_IRRADIANCE
    percentage = float(DARK_ADC_VALUE - adc_value) / (DARK_ADC_VALUE - BRIGHT_ADC_VALUE)
    irradiance = DARK_IRRADIANCE + percentage * (BRIGHT_IRRADIANCE - DARK_IRRADIANCE)
    
    # Begrenze auf sinnvollen Bereich
    irradiance = max(0, min(irradiance, BRIGHT_IRRADIANCE))
    return irradiance

def download_data():
    url = "http://192.168.178.157/download"
    file_path = "esp32_data.csv"
    try:
        with st.spinner('Lade Daten vom ESP32 herunter...'):
            response = requests.get(url, stream=True, timeout=30)
            if response.status_code == 200:
                with open(file_path, "wb") as f:
                    for chunk in response.iter_content(chunk_size=8192):
                        if chunk:
                            f.write(chunk)
                st.success(f"Daten erfolgreich in '{file_path}' gespeichert!")
                return True
            else:
                st.error(f"Fehler beim Herunterladen: Status Code {response.status_code}")
                return False
    except Exception as e:
        st.error(f"Fehler beim Herunterladen: {str(e)}")
        return False

def create_plots(df):
    if df.empty:
        st.warning("Keine Daten im ausgewählten Zeitraum vorhanden.")
        return None

    try:
        fig, axs = plt.subplots(2, 2, figsize=(15, 10))
        fig.suptitle('ESP32 Sensordaten über Zeit (Ausgewählter Zeitraum)')

        # Datumsformatierer für die x-Achse (nur Zeit)
        time_formatter = mdates.DateFormatter('%H:%M')

        plot_params = {
            'marker': 'o',
            'markersize': 1,
            'linewidth': 1,
            'alpha': 0.7
        }

        # Bodenfeuchte
        axs[0, 0].plot(df['Zeitstempel'], df['Bodenfeuchte-Rohwert'], **plot_params)
        axs[0, 0].set_title('Bodenfeuchte')
        axs[0, 0].set_xlabel('Zeit')
        axs[0, 0].set_ylabel('Rohwert')
        axs[0, 0].grid(True, alpha=0.3)
        axs[0, 0].tick_params(axis='x', rotation=45)
        axs[0, 0].xaxis.set_major_formatter(time_formatter)

        # Licht -> Umgestellt auf Lichtintensität
        axs[0, 1].plot(df['Zeitstempel'], df['Lichtintensität (W/m²)'], color='orange', **plot_params)
        axs[0, 1].set_title('Lichtintensität')
        axs[0, 1].set_xlabel('Zeit')
        axs[0, 1].set_ylabel('W/m²')
        axs[0, 1].grid(True, alpha=0.3)
        axs[0, 1].tick_params(axis='x', rotation=45)
        axs[0, 1].xaxis.set_major_formatter(time_formatter)

        # Temperatur
        axs[1, 0].plot(df['Zeitstempel'], df['Temperatur(C)'], color='red', **plot_params)
        axs[1, 0].set_title('Temperatur')
        axs[1, 0].set_xlabel('Zeit')
        axs[1, 0].set_ylabel('°C')
        axs[1, 0].grid(True, alpha=0.3)
        axs[1, 0].tick_params(axis='x', rotation=45)
        axs[1, 0].xaxis.set_major_formatter(time_formatter)

        # Luftfeuchtigkeit
        axs[1, 1].plot(df['Zeitstempel'], df['Luftfeuchtigkeit(%)'], color='blue', **plot_params)
        axs[1, 1].set_title('Luftfeuchtigkeit')
        axs[1, 1].set_xlabel('Zeit')
        axs[1, 1].set_ylabel('%')
        axs[1, 1].grid(True, alpha=0.3)
        axs[1, 1].tick_params(axis='x', rotation=45)
        axs[1, 1].xaxis.set_major_formatter(time_formatter)

        plt.tight_layout(rect=[0, 0.03, 1, 0.95])
        return fig

    except Exception as e:
        st.error(f"Fehler beim Erstellen der Plots: {str(e)}")
        return None

st.set_page_config(layout="wide")
st.title("ESP32 Sensor Daten Visualisierung")

data_file = "esp32_data.csv"

if st.button("Aktuelle Daten vom ESP32 laden"):
    download_data()
    st.rerun()

if os.path.exists(data_file):
    try:
        df = pd.read_csv(data_file)

        required_columns = ['Zeitstempel', 'Bodenfeuchte-Rohwert', 'Licht-Rohwert', 'Temperatur(C)', 'Luftfeuchtigkeit(%)']
        if not all(col in df.columns for col in required_columns):
            st.error(f"Die CSV-Datei '{data_file}' hat nicht die erwarteten Spalten. Bitte lade die Daten erneut herunter.")
            st.stop()

        try:
            df['Zeitstempel'] = pd.to_datetime(df['Zeitstempel'])
        except Exception as e:
            st.error(f"Fehler beim Konvertieren der 'Zeitstempel'-Spalte: {e}. Stelle sicher, dass das Format korrekt ist.")
            st.dataframe(df.head())
            st.stop()

        # Berechne Lichtintensität
        df['Lichtintensität (W/m²)'] = df['Licht-Rohwert'].apply(convert_to_irradiance)

        df.dropna(subset=['Zeitstempel'], inplace=True)

        if df.empty:
            st.warning("Die Datendatei ist leer oder enthält keine gültigen Zeitstempel.")
            st.stop()

        df = df.sort_values(by='Zeitstempel')

        st.sidebar.header("Zeitraum auswählen")
        min_time = df['Zeitstempel'].min()
        max_time = df['Zeitstempel'].max()

        options_list = df['Zeitstempel'].dt.to_pydatetime().tolist()
        
        if len(options_list) > 1:
            selected_range = st.sidebar.select_slider(
                "Zeitraum auswählen:",
                options=options_list,
                value=(options_list[0], options_list[-1]),
                format_func=lambda dt: dt.strftime('%Y-%m-%d %H:%M:%S')
            )
            start_time = selected_range[0]
            end_time = selected_range[1]
        elif len(options_list) == 1:
            st.sidebar.info("Nur ein Datenpunkt vorhanden. Zeige diesen Punkt.")
            start_time = options_list[0]
            end_time = options_list[0]
        else:
            st.sidebar.error("Keine gültigen Zeitstempel für den Slider gefunden.")
            st.stop()

        filtered_df = df[(df['Zeitstempel'] >= start_time) & (df['Zeitstempel'] <= end_time)]

        st.header("Gefilterte Daten")
        st.dataframe(filtered_df)

        st.header("Visualisierung")
        fig = create_plots(filtered_df)
        if fig:
            st.pyplot(fig)
        else:
            st.warning("Konnte keine Visualisierung erstellen.")

    except pd.errors.EmptyDataError:
        st.error(f"Die Datei '{data_file}' ist leer. Bitte lade die Daten zuerst herunter.")
    except FileNotFoundError:
        st.info("Datendatei nicht gefunden. Bitte lade die Daten zuerst herunter.")
    except Exception as e:
        st.error(f"Ein unerwarteter Fehler ist aufgetreten: {str(e)}")

else:
    st.info(f"Keine Daten gefunden ('{data_file}'). Bitte klicke auf 'Aktuelle Daten vom ESP32 laden', um die Daten herunterzuladen.") 