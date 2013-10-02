import socket
import json

data = {"id": 0, "method": "mining.subscribe", "params": []}



for i in range(0,10000):
   s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
   s.connect(('127.0.0.1', 9061))
   data['id']=i
   print data['id'],'->[]->',
   s.send(json.dumps(data)+'\n')
   resoult = s.recv(1024)
   print json.loads(resoult)['id']
   s.close()
