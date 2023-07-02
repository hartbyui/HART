import csv
import os
import sys

csvDir = r'%s' % sys.argv[1]

#csvDir = (r"C:\Users\caleb\OneDrive\Desktop\ALTA")

directorys = []

time = []
coordnets = []
speed = []
angle = []
altatude = []

dataDate = ''

def exportData(dataDate):
    date = (str(dataDate).replace(r'/','-'))
    filename = "ALTA_Data_{0}".format(date)
    fileLocation = csvDir + r'%s' % '\\' + filename
    f = open('%s.csv' % fileLocation, 'w')

    f.write('time, Longitude, Longitude, Speed(Kn), Angle(°), Altatude(m), date,' + dataDate + '\n')
    f.close()
    
    with open('%s.csv' % fileLocation, mode='a', newline='') as csv_file:
        csv_file = csv.writer(csv_file, delimiter=',')
        for i in range(len(coordnets)):
            csv_file.writerow([time[i],coordnets[i][0],coordnets[i][1],speed[i],angle[i],altatude[i]])

def getDirs():
    for filename in os.listdir(csvDir):
        f = os.path.join(csvDir, filename)
        # checking if it is a file
        if os.path.isfile(f):
            tester = f.split('\\')
            length = len(tester)
            tester = tester[length -1][0:3]
            if f != csvDir + r'%s' % '\\' + 'ALTAConcat.py' and tester == 'GPS':
                directorys.append(str(f))

def getData():
    global dataDate
    for i in directorys:
        if os.stat(i).st_size != 0:
            with open((i)) as csv_file:
                csv_reader = csv.reader(csv_file, delimiter=',')
                line_count = 0
                for row in csv_reader:
                    time_ = str(row[0])
                    coords = (str(row[4][11:18].strip()), str('-'+row[5].strip()))
                    speed_ = str(row[8].strip())
                    angle_ = str(row[9][7:].strip())
                    altatude_ = str(row[10][10:].strip())
                    if dataDate != row[1][7:]:
                        dataDate = row[1][7:]
                    coordnets.append(coords)
                    speed.append(speed_)
                    angle.append(angle_)
                    altatude.append(altatude_)
                    time.append(time_)
                    print(time_, coords, '', row[8].strip(), '', row[9][7:].strip(), '', row[10][10:].strip())
                    
                    line_count += 1

getDirs()
getData()
print()
exportData(dataDate)