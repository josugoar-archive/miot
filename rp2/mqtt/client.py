import random
import time
import paho.mqtt.client as mqtt

IDENTIFICADORES = ["EDIFICIO_TUPUESTODELABORATORIO"]
PLANTAS = ["P_B", "P_1", "P_2", "P_3", "P_4", "P_5"]
ALAS = ["N", "S", "E", "O"]
SALAS = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "VIP"]
SENSORES = ["TEMP", "HUM", "LUX", "VIBR"]


client = mqtt.Client()
client.connect("test.mosquitto.org", 1883, 60)
while True:
    identificador = random.choice(IDENTIFICADORES)
    planta = random.choice(PLANTAS)
    ala = random.choice(ALAS)
    sala = random.choice(SALAS)
    sensor = random.choice(SENSORES)
    topic = f"MIOT/{identificador}/{planta}/{ala}/{sala}/{sensor}"
    valor = random.random()
    data = f'{{"{sensor}": {valor}}}'
    client.publish(topic, data)
    client.loop()
    print(topic, data)
    time.sleep(1)
