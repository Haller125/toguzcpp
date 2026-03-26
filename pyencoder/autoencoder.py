import torch
from torch import Tensor, nn


class ClippedReLU(nn.Module):
    def forward(self, input: Tensor) -> Tensor:
        """
        Runs the forward pass.
        """
        return torch.clamp(input, min=0, max=1)

    
class AutoEncoder(nn.Module):
    def __init__(self, input_size, hidden_size):
        super(AutoEncoder, self).__init__()
        self.encoder = nn.Sequential(
            nn.Linear(input_size, hidden_size, bias=False),
            ClippedReLU(),
        )
        self.decoder = nn.Sequential(
            nn.Linear(hidden_size, input_size, bias=False),
        )

    def forward(self, x):
        x = x.float()  
        encoded = self.encoder(x)
        decoded = self.decoder(encoded)
        return decoded
    
