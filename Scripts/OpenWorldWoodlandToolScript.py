"""Sandboxed MCP ProgrammaticToolset script; creates one owned PCG graph."""
import json

GRAPH={'refPath':'/Game/Rogue10m/World/OpenWorld/PCG/PCG_OpenWorldWoodland.PCG_OpenWorldWoodland'}

def sub(name,path,params,x,y=0):
    return execute_tool('PCGToolset.PCGToolset.AddSubgraphNode',json.dumps({
        'graph':GRAPH,'subGraphForNode':{'refPath':path+'.'+path.rsplit('/',1)[-1]},
        'nodeName':name,'jsonParams':json.dumps(params),'nodeTitle':name,
        'nodeComment':'OpenRunMap vegetation; original Epic primitive is referenced.',
        'xPositionIdx':x,'yPositionIdx':y}))['returnValue']

def native(name,kind,params,x,y=0):
    return execute_tool('PCGToolset.PCGToolset.AddNode',json.dumps({
        'graph':GRAPH,'nativeNodeType':kind,'nodeName':name,'jsonParams':json.dumps(params),
        'nodeTitle':name,'nodeComment':'','xPositionIdx':x,'yPositionIdx':y}))['returnValue']

def wire(a,pin,b,target):
    return execute_tool('PCGToolset.PCGToolset.ConnectNodePins',json.dumps({
        'fromNode':a,'fromPinLabel':pin,'toNode':b,'toPinLabel':target}))['returnValue']

def params(items):
    return execute_tool('PCGToolset.PCGToolset.SetGraphParams',json.dumps({'graph':GRAPH,'params':items}))['returnValue']

def run():
    definitions=[]
    for name,kind,container,value,description in [
        ('ForestCenter','Vector','None',{'x':0,'y':0,'z':0},'World location of a legal forest tile'),
        ('TileWidth','Double','None',9000,'Tile side length in centimeters'),
        ('Density','Double','None',0.0024,'Trees per square meter'),
        ('MeshAssets','SoftObjectPath','Array',[
            {'refPath':'/Game/Stylized_Village/Assets/Foliage/Trees/Tree_Medium_Green.Tree_Medium_Green'},
            {'refPath':'/Game/Stylized_Village/Assets/Foliage/Trees/Tree_Large_Green.Tree_Large_Green'},
            {'refPath':'/Game/Stylized_Village/Assets/Foliage/Trees/Tree_Medium_Yellow.Tree_Medium_Yellow'}],
            'Purchased tree references; override per biome')]:
        definitions.append({'name':name,'type':kind,'containerType':container,
                            'defaultValueJson':json.dumps(value),'description':description})
    if not params(definitions):
        raise RuntimeError('Graph parameter creation failed')
    shape=sub('ForestTile','/PCGPrimitives/Primitives/Create/Create_Spline_Shape',
              {'width':9000,'length':9000,'ngon':4,'linear':True,'atWorldOrigin':True},0)
    fill=sub('ScatterTrees','/PCGPrimitives/Primitives/Fill/Fill_Areas_Randomly',
             {'pointsPerSquaredMeter':0.0024,'surfaceRotation':0,
              'rotationRange':{'pitch':0,'yaw':180,'roll':0},
              'samplingPointExtents':{'x':35,'y':35,'z':35},'finalCount':40},300)
    landscape=native('ActualLandscape','Get Landscape Data',{'bUnbounded':True},300,350)
    project=sub('ProjectOnTerrain','/PCGPrimitives/Primitives/Transform/Transform_Project',
                {'projectPositions':True,'projectRotations':False,'projectScales':False},600)
    spawn=sub('SpawnPurchasedTrees','/PCGPrimitives/Primitives/Spawn/Spawn_Assets',
              {'collision':True,'scaleOffset':{'x':0.8,'y':0.8,'z':0.8},'uniformScale':True,
               'randomScaleRange':{'x':0.25,'y':0.25,'z':0.25}},900)
    center=native('CenterParameter','Get Graph Parameter',{'propertyPath':'ForestCenter'},-300,-250)
    width=native('WidthParameter','Get Graph Parameter',{'propertyPath':'TileWidth'},-300,150)
    density=native('DensityParameter','Get Graph Parameter',{'propertyPath':'Density'},0,350)
    assets=native('AssetsParameter','Get Graph Parameter',{'propertyPath':'MeshAssets'},650,550)
    wire(shape,'Out',fill,'PrimaryInput')
    wire(fill,'Out',project,'PrimaryInput')
    wire(landscape,'Out',project,'SecondaryInput')
    wire(project,'Out',spawn,'PrimaryInput')
    wire(center,'Out',shape,'Position')
    wire(width,'Out',shape,'Width')
    wire(width,'Out',shape,'Length')
    wire(density,'Out',fill,'PointsPerSquaredMeter')
    wire(assets,'Out',spawn,'Assets')
    return {'graph':GRAPH,'nodes':9,'parameters':4,'executed':False}
