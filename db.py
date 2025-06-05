# db.py
from motor.motor_asyncio import AsyncIOMotorClient
import os

MONGODB_URI = os.getenv("MONGODB_URI")
if not MONGODB_URI:
    raise RuntimeError("MONGODB_URI is not set")

client = AsyncIOMotorClient(MONGODB_URI)
db = client["locker_system"]
