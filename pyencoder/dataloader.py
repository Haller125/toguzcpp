import torch
import numpy as np
from torch.utils.data import Dataset, DataLoader

# 18*40 + 2*9 + 2*82 = 902
class LargeBinaryDataset(Dataset):
    def __init__(self, file_path, dtype=np.dtype(('u1', 22)), input_size=902):
        self.file_path = file_path
        self.dtype = dtype
        self.input_size = input_size

        # Get number of samples without keeping memmap open
        temp_data = np.memmap(file_path, dtype=self.dtype, mode='r')
        self.num_samples = len(temp_data)
        del temp_data
    def __len__(self):
        return self.num_samples

    def __getitem__(self, idx):
        temp_data = np.memmap(self.file_path, dtype=self.dtype, mode='r')
        datapoint = temp_data[idx].astype(np.uint32)  
        del temp_data

        datapoint[0:18] = np.clip(datapoint[0:18], 0, 39)
        datapoint[20:22] = np.clip(datapoint[20:22], 0, 81)

        sample = np.zeros(self.input_size, dtype=np.float16)
        
        sample = transform_data(datapoint, sample)

        return torch.from_numpy(sample)
    

def transform_data(datapoint, sample):  

    for i in range(0, 18):
        sample[i*40+datapoint[i]] = 1

    offset = 18 * 40
    for i in range(18, 20):
        if datapoint[i] == 18: # нет туздека 
            sample[offset + (i-18)*9] = 1
            continue 
        # sample[offset + (i-18)*9 + datapoint[i]] = 1
        if i == 18: # туздек первого игрока
            sample[offset  + datapoint[i] - 9] = 1 # здесь -9, потому что туздек первого игрока может быть только в клетках 10-18
        else: # туздек второго игрока
            sample[offset + 9 + datapoint[i]] = 1 # здесь нет -9, потому что туздек второго игрока может быть в клетках 0-8
    offset += 2 * 9
    for i in range(20, 22):
        sample[offset + (i-20)*82 + datapoint[i]] = 1

    return sample

def get_data_loader(file_path, batch_size=128, num_workers=1):
    dataset = LargeBinaryDataset(file_path)

    return DataLoader(
        dataset,
        batch_size=batch_size,
        shuffle=False,
        num_workers=num_workers,
        pin_memory=torch.cuda.is_available(),
    )
