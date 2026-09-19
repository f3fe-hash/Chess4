import torch
import torch.nn as nn


class EvalNet(nn.Module):
    def __init__(self):
        super().__init__()

        self.network = nn.Sequential(
            nn.Linear(13 * 64, 128),
            nn.LeakyReLU(0.6),

            nn.Linear(128, 8),
            nn.LeakyReLU(0.5),

            nn.Linear(8, 8),
            nn.LeakyReLU(0.4),

            nn.Linear(8, 1)
        )

    def forward(self, x):
        return self.network(x)
