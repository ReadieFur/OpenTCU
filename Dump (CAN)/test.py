class foo:
    pass

class bar(foo):
    pass

class baz(bar):
    pass

def get_subclasses(cls):
    return cls.__subclasses__() + [g for s in cls.__subclasses__() for g in get_subclasses(s)]

def get_parent_classes(cls):
    if (isinstance(cls, type) == False):
        cls = type(cls)
    list = []
    for superclass in cls.__bases__:
        list.append(superclass)
        list.extend(get_parent_classes(superclass))
    return list

print(get_parent_classes(baz))
print(get_parent_classes(baz()))
