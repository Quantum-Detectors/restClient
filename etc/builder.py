from iocbuilder import Device

__all__ = ['restClient']


class restClient(Device):

    """Create a restClient"""

    # Device attributes
    LibFileList = ['restClient', 'frozen']
    AutoInstantiate = True
