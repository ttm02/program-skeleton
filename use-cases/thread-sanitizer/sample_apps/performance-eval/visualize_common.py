# python common shared data and functions

import numpy as np
from matplotlib.scale import FuncScale


class visualize_common:
    def __init__(self, app=True):
        for ml in self.mode_list:
            self.mode_mapping[ml] = "TSAN + " + ml.replace("+", " + ")

        self.mode_order = list(self.mode_mapping.values())
        self.mode_to_color = dict(zip(self.mode_mapping.keys(), self.colors))
        self.mode_to_color_plot = dict(zip(self.mode_order, self.colors))
        self.mode_apply_list_all = list(self.mode_mapping.keys())

        if not app:
            self.mode_order.remove("vanilla")
            self.mode_apply_list_all.remove("vanilla")

    colors = [
        "#FF0000",
        "#FF00FF",
        "#8B4513",
        "#FF7F50",
        "#FF69B4",
        "#FFA500",
        "#708090",
        "#FFD700",
        "#FFFF00",
        "#00BFFF",
        "#0000FF",
        "#00FF00",
        "#00FFFF",
        "#7FFF00",
        "#008080",
        "#800080",
        "#BA55D3",
        "#D3BA55",
    ]
    mode_list = [
        "loop",
        "merge",
        "merge+loop",
        "single",
        "single+loop",
        "single+merge",
        "single+merge+loop",
        "slicing",
        "slicing+loop",
        "slicing+merge",
        "slicing+merge+loop",
        "slicing+single",
        "slicing+single+loop",
        "slicing+single+merge",
        "slicing+single+merge+loop",
    ]
    mode_mapping = {
        "vanilla": "vanilla",
        "orig": "TSAN",
        "passthrough": "TSAN (pass, but all disabled)",
    }

    def set_mode_lists(self, mal_list):
        mode_to_color_plot_all = self.mode_to_color_plot.copy()
        self.mode_to_color_plot = {}
        self.mode_order = []
        for v in mal_list:
            vra = self.mode_mapping[v]
            self.mode_to_color_plot[vra] = mode_to_color_plot_all[vra]
            self.mode_order.append(vra)

    @staticmethod
    def get_level_scale(ax, x_levels):

        # Forward: value -> position
        def forward(x):
            return np.interp(x, x_levels, np.arange(len(x_levels)))

        # Inverse: position -> value
        def inverse(x):
            return np.interp(x, np.arange(len(x_levels)), x_levels)

        return FuncScale(ax, (forward, inverse))
