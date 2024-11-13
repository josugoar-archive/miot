import json
import paho.mqtt.client as mqtt

UMBRAL = 0.5


def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

    client.subscribe("MIOT/#")


def on_message(client, userdata, msg):
    print(msg.topic + " " + str(msg.payload))
    _, identificador, planta, ala, sala, sensor = msg.topic.split("/")
    data = json.loads(msg.payload)
    if data[sensor] > UMBRAL:
        print(
            f"ALARMA!! identificador: {identificador}, planta: {planta}, ala: {ala}, sala: {sala}, sensor: {sensor}, valor: {data[sensor]}"
        )


client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("test.mosquitto.org", 1883, 60)
client.loop_forever()
