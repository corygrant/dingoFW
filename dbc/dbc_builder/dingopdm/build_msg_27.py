import cantools


def build_msg_27(base_id, name="dingoPdmMsg27"):
    message = cantools.database.Message(
        frame_id=base_id + 27,
        name=name,
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    def enum_signal(sig_name, start, length, choices):
        sig = cantools.database.Signal(
            name=sig_name,
            start=start,
            length=length,
            byte_order="little_endian",
            is_signed=False,
            minimum=0,
            maximum=(1 << length) - 1
        )
        sig.choices = choices
        return sig

    message.signals.append(enum_signal("IgnitionState", 0, 4, {
        0: "Off",
        1: "Ignition",
        2: "Cranking",
        3: "Running"
    }))

    message.signals.append(enum_signal("IgnitionRole", 4, 4, {
        0: "Standalone",
        1: "Master",
        2: "Follower"
    }))

    message.signals.append(enum_signal("DashState", 8, 8, {
        0: "Off",
        1: "On",
        2: "Grace",
        3: "Halting",
        4: "Restart"
    }))

    message.signals.append(enum_signal("SleepStatus", 16, 8, {
        0: "Disabled",
        1: "Awake",
        2: "Counting",
        3: "Following",
        4: "WaitingDash",
        5: "BlockedUsb",
        6: "Quiet",
        7: "Sleep"
    }))

    message.signals.append(enum_signal("MasterLink", 24, 8, {
        0: "NotFollower",
        1: "Ok",
        2: "Lost"
    }))

    message.signals.append(cantools.database.Signal(
        name="SleepCountdown",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        minimum=0,
        maximum=65535,
        unit="s"
    ))

    def bit_signal(sig_name, start):
        return cantools.database.Signal(
            name=sig_name,
            start=start,
            length=1,
            byte_order="little_endian",
            is_signed=False,
            minimum=0,
            maximum=1
        )

    for bit, sig_name in enumerate(["IgnitionOut", "AccessoryOut", "DashOut", "StarterOut"]):
        message.signals.append(bit_signal(sig_name, 48 + bit))

    # What woke the device from its last sleep, all 0 after a power on or reset
    for start, sig_name in ((56, "WakeCan"), (57, "WakeDigitalInput"), (58, "WakeUsb"),
                            (59, "WakeOtherLine"), (60, "WakeInterrupt"), (63, "WokeFromSleep")):
        message.signals.append(bit_signal(sig_name, start))

    return message
