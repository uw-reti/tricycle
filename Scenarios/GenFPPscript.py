# Generate xml snippet files with for FPP specification and deployment

import csv
import argparse

#Enter FPP spec and Deployment commissioning input files

def add_parse():    
    parser = argparse.ArgumentParser(description='Generate FPP deployment scenario for tricycle')
    parser.add_argument('-f','--fpp', type=str, help='FPP spec file')
    parser.add_argument('-d','--dep', type=str, help='FPP deployment file')
    return parser.parse_args()

# function to turn csv file into list

def read_csv_to_list(filename):
    """
    Reads a CSV file into a python list of dictionaries with dictionary keys 
    based on the CSV header values
    """

    data = []

    with open(filename, mode='r', newline='', encoding='utf-8') as file:
        reader = csv.DictReader(file)
        for row in reader:
            data.append(row)

    return data

def process_deployment_data(data_list):

    deployment_map = {}

    for entry in data_list:
        region = entry['region_name']
        if region not in deployment_map:
            deployment_map[region] = {}
        institution = entry['institution']
        if institution not in deployment_map[region]:
            deployment_map[region][institution] = {'prototypes': [], 'build_times': [], 'lifetimes': [], 'n_build': []}
        for key in deployment_map[region][institution].keys():
            deployment_map[region][institution][key].append(entry[key])

    return deployment_map

def fill_region_template(region_name, institution_data, template_filename, inst_template_filename):
    """
    Fill in a region template with data from a dictionary for that region. That
    dictionary is expected to have institution names as keys, and each value is
    expected to be a dictionary with four keys: prototypes, build_times,
    lifetimes, n_build. Each of those keys should have a list of values.
    """

    with open(template_filename, 'r') as template_file:
        template = template_file.read()

    institution_strings = []
    for inst_name, data_dict in institution_data.items():
        inst_string = fill_institution_template(inst_name, data_dict, inst_template_filename)
        institution_strings.append(inst_string)

    institutions = '\n'.join(institution_strings)

    xml_string = template.format(region_name=region_name, institutions=institutions)

    return xml_string       

def fill_institution_template(inst_name, data_dict, template_filename):
    """
    Fill in an institution template with data from a dictionary for that
    institution. That dictionary is expected to have four keys: prototypes,
    build_times, lifetimes, n_build. Each of those keys should have a list of
    values.
    """

    with open(template_filename, 'r') as template_file:
        template = template_file.read()

    prototypes = '\n'.join([f"<val>{p}</val>" for p in data_dict['prototypes']])
    build_times = '\n'.join([f"<val>{bt}</val>" for bt in data_dict['build_times']])
    lifetimes = '\n'.join([f"<val>{lt}</val>" for lt in data_dict['lifetimes']])            
    n_build = '\n'.join([f"<val>{nb}</val>" for nb in data_dict['n_build']])
    

    xml_string = template.format(inst_name=inst_name, prototypes=prototypes, 
                                 build_times=build_times, lifetimes=lifetimes, 
                                 n_build=n_build)

    return xml_string

def build_XML_snippet():
    """
    Main function to build the FPP and Deployment XML snippets. Reads in the FPP
    spec and deployment commissioning timeline from csv files, fills in the
    template files, and writes the resulting XML snippets to output files.                         
    """
    args = add_parse()

    fpp_list = read_csv_to_list(args.fpp)
    # fill the FPP template file with specs
    with open('FPP.tmpl', 'r') as template_file:
        template = template_file.read()
    fpp_xml_list = []
    for fpp in fpp_list:
        fpp_xml_list.append(template.format(**fpp))
    fpp_string = '\n\n'.join(fpp_xml_list)
    with open('FPP.xml', 'w') as fpp_out:
        fpp_out.write(fpp_string)

    # fill the Deployment template with regions and commisioning timeline
    dep_list = read_csv_to_list(args.dep)
    dep_map = process_deployment_data(dep_list)
    with open('Deployment.xml', 'w') as dep_out:
        for region_name, institution_data in dep_map.items():
            region_xml = fill_region_template(region_name, institution_data, 'Region.tmpl', 'Institution.tmpl')
            dep_out.write(region_xml + "\n\n") 

# turn the spec and timeline files into lists
if __name__ == '__main__':
    build_XML_snippet()
