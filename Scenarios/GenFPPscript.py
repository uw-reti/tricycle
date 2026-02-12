# Generate xml snippet files with for FPP specification and deployment

import csv
import argparse

#Enter FPP spec and Deployment commissioning input files

parser = argparse.ArgumentParser(description='Generate FPP deployment scenario for tricycle')
parser.add_argument('-f','--fpp', type=str, help='FPP spec file')
parser.add_argument('-d','--dep', type=str, help='FPP deployment file')
args = parser.parse_args()

# function to turn csv file into list

def read_csv_to_list(filename):
    """
    Reads a CSV file into a python list
    """

    data = []

    with open(filename, 'r') as fp:
        flines =sum(1 for line in fp)
    
    with open(filename, mode='r', newline='', encoding='utf-8') as file:
        for row in range(flines):
            reader = csv.reader(file)
            for row in reader:
                data.append(row)
    return data[1:row+1] # Returns the values of input data in lists

# turn the spec and timeline files into lists
if __name__ == '__main__':
    fpp_list = read_csv_to_list(args.fpp)
    dep_list = read_csv_to_list(args.dep)

# fill the FPP template file with specs
with open("FPP_spec.xml", "w") as fplant:
     for i in range(len(fpp_list)):
         fppspec = open('FPPTemplate.txt', 'r').read().format(*fpp_list[i])
         fplant.write(fppspec)
         
# fill the Deployment template with regions and commisioning timeline
with open("FPP_dep.xml", "w") as comm:
   for i in range(len(dep_list)):
    dep_time = open('Deployment.txt', 'r').read().format(*dep_list[i])
    comm.write(dep_time)
