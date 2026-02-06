import csv

# enter the csv or text file of FPP data

FPP = input("Enter FPP file name: ")
Deploy = input("Enter deployment timeline file name: ")

def read_csv_to_list(filename):
    """
    Reads a CSV file into a python list
    """

    data = []

    with open(filename, mode='r', newline='', encoding='utf-8') as file:
        reader = csv.reader(file)
        for row in reader:
            data.append(row)
    return data[1] # Returns the values of FPP input data

csv_list = read_csv_to_list(FPP)

f = open('FPPTemplate.txt', 'r')
plant = f.read()
formatted = plant.format(*csv_list)

with open("FPP.xml", "w") as c:
    c.write(formatted)

timeline = read_csv_to_list(Deploy)

t = open('Deployment.txt', 'r')
time = t.read()
comm = time.format(*timeline)

with open("FPP_Deployment.xml", "w") as d:
    d.write(comm)
