import pyautogui as pag
import time

pag.FAILSAFE = False

sizeVals = pag.size()
pag.moveTo(sizeVals.width-1,sizeVals.height-1)
time.sleep(1)
pag.click()
#pag.moveRel(-5000,5000)