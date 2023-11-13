import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

data = pd.read_csv('jitter_data.csv')
data = data[(data['0'] < 120000 ) & (data['0'] != 0)]
sns.scatterplot(range(len(data)), data['0'], marker='x', s=1)
plt.ylim(114980, 115020)
plt.savefig('jitter.png')