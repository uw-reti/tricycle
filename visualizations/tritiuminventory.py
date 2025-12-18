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

year = date.InitialYear[0]

plt.plot(year + tritium.Time/12, tritium.Quantity)

#Add labels, axes, and title
plt.xlabel("Year")
plt.xlim(year,year+75)
plt.ylabel("Tritium [kg]")
plt.ylim(0,50)
plt.title("Tritium Inventory")

#Show the plot
plt.show()
