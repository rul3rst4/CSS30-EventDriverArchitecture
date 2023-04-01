import sys
import signal
from random import choice
import matplotlib.pyplot as plt
import numpy as np
from argparse import ArgumentParser, FileType
from configparser import ConfigParser
from confluent_kafka import Consumer, OFFSET_BEGINNING
from time import sleep
import threading
import json
from matplotlib.animation import FuncAnimation
import time
import datetime
import matplotlib.dates as mdates

lock = threading.Lock()

def load_topics(config):
    with open(config['topics_file']) as f:
        return json.load(f)

fig = plt.figure()
ax = plt.subplot(1,1,1)
x_data = []
y_data = []

line, = ax.plot(x_data, y_data)
ax.set_xlabel('Time')
ax.set_ylabel('Value')
ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))


def run_consumer(config, topic):
    topic_name = topic['Name']
    topic_acronym = topic['Acronym']
    print(f'Starting consumer for topic {topic_name} - {topic_acronym}')
    consumer = Consumer(config)
    consumer.subscribe([topic['Acronym']])
    
    try:
        while True:
            msg = consumer.poll(1.0)
            if msg is None:
                # Initial message consumption may take up to
                # `session.timeout.ms` for the consumer group to
                # rebalance and start consuming
                print("Waiting...")
            elif msg.error():
                print(f"ERROR: {msg.error()}")
            else:
                # Extract the (optional) key and value, and print.
                print("Consumed event from topic {topic}: value = {value:12}".format(
                    topic=msg.topic(), value=msg.value().decode('utf-8')))
                value = float(msg.value().decode('utf-8'))
                if topic['Acronym'] == "BITCOIN":
                    lock.acquire()
                    current_time = datetime.datetime.now()
                    x_data.append(current_time)
                    y_data.append(value)
                    lock.release()
    except KeyboardInterrupt:
        pass
    finally:
        # Leave group and commit final offsets
        consumer.close()

def spin_consumers(config, topics):
    for topic in topics:
        t = threading.Thread(target=run_consumer, args=(config, topic), daemon=True)
        t.start()

def update(i):
    lock.acquire()
    ax.clear()
    ax.plot(x_data, y_data)

    now = datetime.datetime.now()
    xmin = now - datetime.timedelta(minutes=10) 
    ax.set_xlim(xmin, now)
    lock.release()

ani = FuncAnimation(fig, update, interval=1000, cache_frame_data=False)

def busy_wait():
    plt.show()
    while True:
    #     lock.acquire()
    #     # y_data.append(np.random.random())
    #     line.set_data(x_data, y_data)

    #     ax.relim()
    #     ax.autoscale_view()

    #     fig.canvas.draw()
    #     fig.canvas.flush_events()
    #     lock.release()
        sleep(1)

def signal_handler(signum, frame):
    res = input("Ctrl-c was pressed. Do you really want to exit? y/n ")
    if res == 'y':
        exit(1)

# Usage example: python .\Consumer\main.py -c .\kafka\config.ini -t .\Topics.json
if __name__ == '__main__':
    parser = ArgumentParser()
    parser.add_argument('-c', '--config', type=FileType('r'), required=True)
    parser.add_argument('-t', '--topics_file', type=FileType('r'), required=True)
    args = parser.parse_args()

    config_parser = ConfigParser()
    config_parser.read_file(args.config)
    config = dict(config_parser['default'])
    config.update(config_parser['consumer'])
    print(config)

    topics : dict = json.load(args.topics_file)
    print(topics["Coins"])
 
    signal.signal(signal.SIGINT, signal_handler)
    spin_consumers(config, topics["Coins"])
    busy_wait()