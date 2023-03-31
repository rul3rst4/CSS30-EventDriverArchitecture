from kafka import KafkaProducer
import requests
import threading
import time


def bitcoin():
    producer = KafkaProducer(bootstrap_servers='localhost:9092')
    while True:
        response = requests.get('https://www.mercadobitcoin.net/api/BTC/ticker/')
        data = response.json()
        producer.send('BITCOIN', bytes(data["ticker"]["buy"], encoding= 'utf-8'))
        time.sleep(10)
def dogecoin():
    producer = KafkaProducer(bootstrap_servers='localhost:9092')
    while True:
        response = requests.get('https://www.mercadobitcoin.net/api/DOGE/ticker/')
        data = response.json()
        producer.send('DOGECOIN', bytes(data["ticker"]["buy"], encoding= 'utf-8'))
        time.sleep(10)

t = threading.Thread(target=bitcoin)
t.start()

t2 = threading.Thread(target=dogecoin)
t2.start()