import yaml
import os

def default_cfg():
    cfg_dict = dict()
    cfg_dict["eye_color_rgb"]=[255,255,225]
    return cfg_dict

#保存yaml配置
def write_yaml_cfg(cfg_dict):
    current_dir = os.path.dirname(os.path.realpath(__file__))
    cfg_file_name = os.path.join(current_dir, "cfg", "cfg.yaml")
    with open(cfg_file_name, 'w') as f:
        yaml.dump(cfg_dict, f)

#读取yaml配置
def read_yaml_cfg():
    current_dir = os.path.dirname(os.path.realpath(__file__))
    cfg_file_name = os.path.join(current_dir, "cfg", "cfg.yaml")
    if not os.path.exists(cfg_file_name): #如果配置不存在，则使用默认配置，并且保存下来
        cfg_dict = default_cfg()
        write_yaml_cfg(cfg_dict)

    if os.path.getsize(cfg_file_name)==0: #如果配置文件大小为0，则使用默认配置，并且保存下来
        cfg_dict = default_cfg()
        write_yaml_cfg(cfg_dict)

    with open(cfg_file_name) as f:
        cfg_dict = yaml.load(f, Loader=yaml.FullLoader)
        #print("cfg_dict=",cfg_dict)

    return cfg_dict

if __name__ == "__main__":
    read_yaml_cfg()
