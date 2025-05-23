#Plot explicit inventory

import numpy as np
import sqlite3
import pandas
import matplotlib.pyplot as plt

filename = str(input("Enter file path:"))
conn = sqlite3.connect(filename)

sql="""SELECT "_rowid_",* FROM "main"."ExplicitInventory" WHERE "NucId" LIKE '%1003%' AND "InventoryName" LIKE '%stocks%' LIMIT 49999 OFFSET 0;"""

data = pandas.read_sql(sql,conn)

plt.plot(1967+data.Time/12, data.Quantity)

#Add labels and title
plt.xlabel("Year")
plt.ylabel("Tritium [kg]")
plt.title("Tritium Inventory")

#Show the plot
plt.show()