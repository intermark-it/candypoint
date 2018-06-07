import endpoints
from google.appengine.ext import ndb
from protorpc import messages
from protorpc import message_types
from protorpc import remote
from endpoints_proto_datastore.ndb import EndpointsModel
import logging
from random import randint

class User(EndpointsModel):
    name = ndb.StringProperty()
    points = ndb.IntegerProperty()
    pin = ndb.IntegerProperty()
    rfid = ndb.StringProperty(required=True)
    active = ndb.BooleanProperty()
    message = ndb.StringProperty()
    openVault = ndb.BooleanProperty()
    _message_fields_schema = ('id', 'name','points','pin','rfid','message','openVault','active')

    @classmethod
    def query_rfid(cls,rfidParam):
        return cls.query(User.rfid == rfidParam)

    @classmethod
    def query_pin(cls,pinParam):
        return cls.query(User.pin == pinParam)

@endpoints.api(name='candy', version='v1')
class CandyApi(remote.Service):

    @User.method(path='user', http_method='PUT', name='user.add')
    def user_add(self, user):
        user.put()
        return user

    @User.query_method(query_fields=('rfid',), path='user/{rfid}', http_method='GET', name='user.get')
    def user_get(self, user):
        if user.get():
            return user
        else:
            raise endpoints.NotFoundException('Usuario no encontrado')

    @User.method(path='user', http_method='POST', name='user.update')
    def user_update(self, user):
        if user.from_datastore:
            user.put()
            return user
        else:
            foundUsers = user.query_rfid(user.rfid).fetch(1)
            if foundUsers:
                foundUser = foundUsers[0]
                user.id=foundUser.id
                if not user.points:
                    user.points = foundUser.points
                if not user.name:
                    user.name = foundUser.name
                user.put()
            else:
                raise endpoints.NotFoundException('Usuario no encontrado')
        return user
    
    @User.method(path='user/find', http_method='GET', name='user.find')
    def user_find(self, requestUser):
        if requestUser.from_datastore:
            return requestUser
        else:
            raise endpoints.NotFoundException('Usuario no encontrado')


    @User.method(path='user/signup', http_method='POST', name='user.signup')
    def user_pin(self, requestUser):
        foundUsers = User().query_rfid(requestUser.rfid).fetch(1)
        if foundUsers:
            logging.error("Signup usuario registrado - %s",requestUser)
            newUser = foundUsers[0]
            newUser.active = True
            if requestUser.points:
                newUser.points = requestUser.points
            if requestUser.name:
                newUser.name = requestUser.name
            newUser.pin = 0
            newUser.put()
        else:
            #logging.error("PIN - %s", requestUser.pin)
            if requestUser.pin:
                foundUser = requestUser.query_pin(requestUser.pin)
                if foundUser:
                    newUser = requestUser
                    newUser.active = True
                    newUser.pin = 0000
                    newUser.put()
                else:
                    raise endpoints.NotFoundException('PIN no encontrado')
            else:
                raise endpoints.NotFoundException('PIN no encontrado')
            
        return newUser

    

    @User.method(path='user/validate/{rfid}', http_method='POST', name='user.validate')
    def user_validate(self, userQuery):
        foundUsers = User().query_rfid(userQuery.rfid).fetch(1)
        if foundUsers:
            logging.error("Usuarios encontrado - %s", foundUsers)
            foundUser = foundUsers[0]
            logging.error("Usuario encontrado - %s", foundUser)
            if foundUser.points > 49:
                foundUser.points -= 50
                foundUser.message = "Caja Abierta"
                foundUser.openVault = True
                foundUser.put()
            else:
                foundUser.openVault = False
                foundUser.message = "Puntos insuficientes"        

            return foundUser
        else:
            user=User()
            user.rfid=userQuery.rfid
            user.active=False
            
            validPin = False
            while not validPin:
                newPin = randint(0,9999)
                logging.error("NUEVO PIN: %s",newPin)
                encontrados = User().query_pin(newPin).fetch(1)
                #logging.error("ENCONTRADOS POR PIN: %s",encontrados)
                if(not encontrados):
                    user.pin = int(format(newPin,'04'))
                    validPin = True

            user.message = "No estas registrado"
            user.openVault = False
            user.put()

            return user



    @User.query_method(query_fields=('name',),path='getUserByName/{name}', name='user.list')
    def get_user_by_name(self, query):
        return query


api = endpoints.api_server([CandyApi])
