import endpoints
from google.appengine.ext import ndb
from protorpc import messages
from protorpc import message_types
from protorpc import remote
from endpoints_proto_datastore.ndb import EndpointsModel

class User(EndpointsModel):
    code = ndb.StringProperty()
    name = ndb.StringProperty()
    points = ndb.IntegerProperty()
    pin = ndb.IntegerProperty()
    rfid = ndb.StringProperty()
    active = ndb.BooleanProperty()
    _message_fields_schema = ('id','code', 'name','points','pin','rfid','active')

class UserMessage(messages.Message):
    id = messages.StringField(1)
    email = messages.StringField(2)
    username = messages.StringField(3)

class MyMessageClass(messages.Message):
  id = messages.StringField(1)  # Or any other field type

@endpoints.api(name='candy', version='v1')
class CandyApi(remote.Service):

    @User.method(name='user.insert',
                      path = 'user')
    def insert_user(self, user):
        user.put()
        return user

    @User.method(request_fields=('id',), 
                    path='getUser/{id}', http_method='GET', name='user.get')
    def get_user(self, user):
        if not user.from_datastore:
            user.put()
            #raise endpoints.NotFoundException('Id no encontrado')
        return user

    @User.query_method(query_fields=('rfid',),path='getUserByRfid/{rfid}', name='user.gets')
    def get_user_by_rfid(self, userQuery):
        if userQuery.get():
            return userQuery
        else:
            user=User()
            user.rfid=userQuery.get_unrecognized_field_info('rfid')
            user.active=False
            user.pin=1234
            user.put()

        return userQuery

    @User.query_method(query_fields=('name',),path='getUserByName/{name}', name='user.list')
    def get_user_by_name(self, query):
        return query


    ID_RESOURCE = endpoints.ResourceContainer(message_types.VoidMessage,
                                          id=messages.StringField(1,
                                                                  variant=messages.Variant.STRING,
                                                                  required=True))
    @endpoints.method(ID_RESOURCE,
                  UserMessage,
                  http_method='GET',
                  path='getUser2/{id}',
                  name='users.read')
    def read(self, request):
        entity = User.get_by_id(request.id)
        if not entity:
            message = 'No User with the id "%s" exists.' % request.id
            raise endpoints.NotFoundException(message)

        return entity.to_message()

api = endpoints.api_server([CandyApi])