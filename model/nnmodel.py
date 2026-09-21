import torch
import torch.nn as nn

INPUT_SIZE = 6 * 64

class EvalNet(nn.Module):
    def __init__(self):
        super().__init__()

        self.network = nn.Sequential(
            nn.Linear(INPUT_SIZE, 8),
            nn.LeakyReLU(0.6),

            #nn.Linear(128, 8),
            #nn.LeakyReLU(0.5),

            nn.Linear(8, 4),
            nn.LeakyReLU(0.4),

            nn.Linear(4, 1)
        )

    def forward(self, x):
        return self.network(x)