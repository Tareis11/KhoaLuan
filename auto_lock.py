import asyncio
from db import db


async def auto_lock_locker(locker_id, delay=10):
    await asyncio.sleep(delay)
    await db.locker.update_one({"_id": locker_id}, {"$set": {"isLocked": True}})
