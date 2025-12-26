import xml.etree.cElementTree as ET
import csv

# function to create list from file
def csv_to_list(file):

    with open(file, newline='', encoding='utf-8') as csvfile:
        reader = csv.reader(csvfile)
        data_list = list(reader)
    return data_list

# creating lists from file
csv_data = csv_to_list('FPP_input.csv')[1]
FPP_data = csv_to_list('FPP_input.csv')[3]

# printing lists to check correct selection, can be commented out
print(csv_data)
print(FPP_data)

tree = ET.parse('SampleFPP.xml')


# substituting values into FPP definition
tree.find("./facility/name").text = csv_data[0]
tree.find(".//fusion_power").text = csv_data[1]
tree.find(".//TBR").text = csv_data[2]
tree.find(".//reserve_inventory").text = csv_data[3]
tree.find(".//sequestered_equilibrium").text = csv_data[4]
tree.find(".//fuel_incommod").text = csv_data[5]
tree.find(".//Li7_contribution").text = csv_data[6]
tree.find(".//refuel_mode").text = csv_data[7]
tree.find(".//buy_quantity").text = csv_data[8]
tree.find(".//buy_frequency").text = csv_data[9]
tree.find(".//he3_outcommod").text = csv_data[10]
tree.find(".//blanket_inrecipe").text = csv_data[11]
tree.find(".//blanket_incommod").text = csv_data[12]
tree.find(".//blanket_outcommod").text = csv_data[13]
tree.find(".//blanket_size").text = csv_data[14]
tree.find(".//blanket_turnover_fraction").text = csv_data[15]
tree.find(".//blanket_turnover_frequency").text = csv_data[16]

# substituting values into region
tree.find("./region/name").text = FPP_data[0]
tree.find("./region/institution/name").text = FPP_data[1]
tree.find(".//prototypes/val").text = csv_data[0]
tree.find(".//build_times/val").text = FPP_data[2]
tree.find(".//lifetimes/val").text = FPP_data[3]
tree.find(".//n_build/val").text = FPP_data[4]

# output file
tree.write("FPPoutput.xml")
