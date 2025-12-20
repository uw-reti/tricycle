#Plot explicit inventory

import numpy as np
import sqlite3
import pandas
import matplotlib.pyplot as plt

#filename = str(input("Enter file path:"))
conn = sqlite3.connect("cyclus.sqlite")

sql="""SELECT "_rowid_",* FROM "main"."ExplicitInventory" WHERE "NucId" LIKE '%1003%' AND "InventoryName" LIKE '%stocks%' LIMIT 49999 OFFSET 0;"""

data = pandas.read_sql(sql,conn)

info = """SELECT "_rowid_",* FROM "main"."Info" LIMIT 1 OFFSET 0;"""

date = pandas.read_sql(info,conn)

year = date.InitialYear[0]

plt.plot(year + data.Time/12, data.Quantity)

#Add labels and title
plt.xlabel("Year")
plt.xlim(year,year+75)
plt.ylabel("Tritium [kg]")
plt.ylim(0,50)
plt.title("Tritium Inventory")

#Show the plot
plt.show()

