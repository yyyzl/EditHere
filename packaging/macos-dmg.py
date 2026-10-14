"""DMG 安装窗口：左侧应用，右侧 Applications，中间为拖拽箭头。"""
from pathlib import Path

application = defines["app"]
files = [application]
symlinks = {"Applications": "/Applications"}
format = "UDZO"
filesystem = "HFS+"
icon = str(Path(application) / "Contents/Resources/edithere.icns")
hide_extensions = ["EditHere.app"]
background = "builtin-arrow"
window_rect = ((100, 100), (640, 280))
default_view = "icon-view"
icon_locations = {"EditHere.app": (140, 120), "Applications": (500, 120)}
icon_size = 128
text_size = 16
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
