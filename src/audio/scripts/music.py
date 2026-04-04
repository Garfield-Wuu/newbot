import os
import psutil
import pulsectl

def play_music(file_name,background_playback=False,volume_factor=1.0):
    # pygame.mixer.init()
    # pygame.mixer.music.set_volume(1.0) #音量0~1
    # pygame.mixer.music.load(file_name)
    # pygame.mixer.music.play()
    #
    # while pygame.mixer.music.get_busy():
    #     time.sleep(0.1)
    #
    # pygame.mixer.music.stop()

    if volume_factor==1.0:
        if background_playback:
            cmd = "play \"%s\" >/dev/null 2>&1 &" % (file_name)
        else:
            cmd = "play \"%s\" >/dev/null 2>&1"%(file_name)
    else:
        if background_playback:
            cmd = "play -v %f \"%s\" >/dev/null 2>&1 &" % (volume_factor,file_name)
        else:
            cmd = "play -v %f \"%s\" >/dev/null 2>&1"%(volume_factor,file_name)
    os.system(cmd)


def check_music_playing():
    for proc in psutil.process_iter(['name']):
        if proc.info['name'] == 'play':
            return True
    return False

def check_ffplay_playing():
    for proc in psutil.process_iter(['name']):
        if proc.info['name'] == 'ffplay':
            return True
    return False

def kill_music_process():
    for proc in psutil.process_iter(['name', 'cmdline']):
        if proc.info['name'] == 'play' and 'play' in proc.info['cmdline']:
            proc.kill()

def set_volume(new_volume):
    pulse = pulsectl.Pulse('volume-control')
    sink_info = pulse.get_sink_by_name('@DEFAULT_SINK@')
    pulse.volume_set_all_chans(sink_info, new_volume/100)
    pulse.close()

#如果用python代码接口设置音量，最大好像只能到153%
def change_volume(volume_increment,min_volume,max_volume):
    # os.system("amixer -D pulse set Master 20%+")
    # os.system("pactl set-sink-volume 1 +20%")
    pulse = pulsectl.Pulse('volume-control')
    sink_info = pulse.get_sink_by_name('@DEFAULT_SINK@')
    current_volume = round(sink_info.volume.value_flat * 100)
    new_volume = current_volume + volume_increment
    if new_volume < min_volume:
        new_volume = min_volume
    elif new_volume > max_volume:
        new_volume = max_volume
    pulse.volume_set_all_chans(sink_info, new_volume/100)
    pulse.close()

def get_current_volume():
    pulse = pulsectl.Pulse('volume-info')
    sink_info = pulse.get_sink_by_name('@DEFAULT_SINK@')
    volume = round(sink_info.volume.value_flat * 100)
    pulse.close()
    return volume
