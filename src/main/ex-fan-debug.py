import os
import sys
import serial.tools.list_ports
from serial.tools.list_ports_linux import SysFS

portInfo=("50.0","50.1") # 0 controller:50.0, 1 controller:50.1
ch341Describe={"posix":"USB Serial","nt":"USB-SERIAL CH340"}
fanInfo=((0,1), # fan 0
(0,2), # fan 1
(0,3), # fan 2
(1,3)) # fan 3

def initPort(info:SysFS) -> serial.Serial:
    port=serial.Serial(port=info.device,baudrate=9600,bytesize=serial.EIGHTBITS,stopbits=serial.STOPBITS_ONE,parity=serial.PARITY_NONE)
    return port

def searchPort(index:int):
    print("Searching port...")
    ports=list(serial.tools.list_ports.comports())
    for i in ports:
        if i.description.find(ch341Describe[os.name])!=-1:
            with initPort(i) as port:
                port.write(b"read")
                curFreq=port.readline().decode("ascii").strip()[1:5]
                print("port:"+i.name+" freq:"+curFreq)
                if curFreq==portInfo[index]:
                    print("MATCH")
                    return i.device

mode=sys.argv[1]
index=int(sys.argv[2])
target=(int(sys.argv[3]) if mode=="s" else 0)
if __name__=="__main__":
    deviceName=searchPort(fanInfo[index][0])
    if deviceName==None:
        print("Failed to find port, exit")
        sys.exit(1)
    else:
        print("Found port: ["+deviceName+"], writing data")
    with serial.Serial(port=deviceName,baudrate=9600,bytesize=serial.EIGHTBITS,stopbits=serial.STOPBITS_ONE,parity=serial.PARITY_NONE) as port:
        if port.is_open:
            if mode=="s":
                port.write(f"D{str(fanInfo[index][1])}:{str(target).zfill(3)}".encode("ascii"))
            elif mode=="q":
                port.write(b"read")
            print("result:")
            print(port.readline().decode("ascii").strip())
        else:
            print(f"fail to open {deviceName}")
