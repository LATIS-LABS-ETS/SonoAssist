import json

from sonopy.file_management import SonoFolderManager


class ConfigurationManager():

    ''' Allows to acces all configuration data from one object '''


    def __init__(self, config_file_path, acquisition_dir_path=None):

        '''
        Params
        ------
        config_path (str) : path to the configuration file
        acquisition_dir_path (str) : path to the acquisition directory
        '''

        self.config_file_path = config_file_path
        self.acquisition_dir_path = acquisition_dir_path

        self.config_data = None

        # laoding config from sources
        self.load_config_file()
        if acquisition_dir_path is not None: 
            self.load_acquisition_output_file()
        

    def load_config_file(self):

        ''' loads configuration data from specified file '''

        with open(self.config_file_path) as config_f:
            self.config_data = json.load(config_f)
            config_f.close()


    def load_acquisition_output_file(self):
        
        ''' Loads configuration from the output parameters file of the acquisition tool '''
        
        folder_manager = SonoFolderManager(self.acquisition_dir_path)
        self.config_data = {**self.config_data, **folder_manager.load_output_params()}


    def __getitem__(self, key):

        ''' Short cut for reading config params '''

        if isinstance(key, str):
            return self.config_data[key]
        else:
            raise ValueError("Key must be a string")


    def __setitem__(self, key, value):

        ''' Short cut for writting config params '''

        if isinstance(key, str):
            self.config_data[key] = value
        else:
            raise ValueError("Key must be a string")


    def __contains__(self, key):

        ''' Short cut for membership check ''' 
        
        if isinstance(key, str):
            return key in self.config_data
        else:
            raise ValueError("Key must be a string")