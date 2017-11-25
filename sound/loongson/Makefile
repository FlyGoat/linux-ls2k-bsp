snd-hda-ls7a-objs := hda_ls7a.o

snd-hda-codec-y := hda_codec.o
snd-hda-codec-$(CONFIG_PROC_FS) += hda_proc.o

snd-hda-codec-realtek-objs :=	patch_realtek.o

# common driver
obj-$(CONFIG_SND_HDA_LS7A) := snd-hda-codec.o

obj-$(CONFIG_SND_HDA_LS7A) += snd-hda-codec-realtek.o

# this must be the last entry after codec drivers;
obj-$(CONFIG_SND_HDA_LS7A) += snd-hda-ls7a.o
