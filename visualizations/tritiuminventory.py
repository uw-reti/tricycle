#Plot tritium in explicit inventory

import numpy as np
import sqlite3
import pandas
import matplotlib.pyplot as plt

#Input file
conn = sqlite3.connect("cyclus.sqlite")

#sql syntax to select only tritium stocks from the explicitinventory table and the startyear
sql="""SELECT "_rowid_",* FROM "main"."ExplicitInventory" WHERE "NucId" LIKE '%1003%' AND "InventoryName" LIKE '%stocks%' LIMIT 49999 OFFSET 0;"""

info = """SELECT "_rowid_",* FROM "main"."Info" LIMIT 1 OFFSET 0;"""

tritium = pandas.read_sql(sql,conn)
date = pandas.read_sql(info,conn)

#start year definition
year1 = date.InitialYear[0]

#converting timesteps from months to years
time = tritium.Time/12

#plotting tritium inventory over time
plt.plot(year1 + time, tritium.Quantity)

#selects maximum amount of tritium plus 5%
apex = 1.05 * max(tritium.Quantity)

#selects maximum time plus 5% for time axis
year2 = year1 + 1.05 * max(time)

#Add labels and title
plt.xlabel("Year")

plt.xlim(year1, year2)
plt.ylabel("Tritium [kg]")
plt.ylim(0, apex)
plt.title("Tritium Inventory")

#Show the plot
plt.show()
