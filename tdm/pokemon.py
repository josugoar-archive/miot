#pip3 install selenium
#pip3 install webdriver_manager
#https://googlechromelabs.github.io/chrome-for-testing/


from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import Select
import time
from sys import platform


def extractPokemonGrid(driver):
    prods = driver.find_element(By.CLASS_NAME, 'products')
    prods = driver.find_elements(By.XPATH, '//ul[@class="products columns-4"]/li')

    #Get a list
    pokemonList = []
    for prod in prods:  
        pokeText = prod.text
        type(pokeText)
        aux = pokeText.split("\n")
        poke = aux[0] +":"+ aux[1]
        pokemonList.append(poke)

    return pokemonList

#configure the driver
print("Configuring the driver\n")
if platform == "linux" or platform == "linux2":
    # linux
    print("In linux platform")
    cService = webdriver.ChromeService(executable_path='./chromedriver')
    driver = webdriver.Chrome(service = cService)
elif platform == "darwin":
    # OS X
    print("In macOS platform")
    driver = webdriver.Chrome()  
elif platform == "win32":
    # Windows...
    print("windows")
    driver = webdriver.Chrome() 



driver.get("https://scrapeme.live/shop/")
print(driver.title)

#Extract a pokemon list from a grid
print("Extracting a pokemon list from a grid\n")
pokemonList = extractPokemonGrid(driver)
print(pokemonList)

#select a dropdown element
print("Selecting a new order\n")
select = Select(driver.find_element(By.NAME,'orderby'))
select.select_by_visible_text('Sort by newness')

#Re-extract a pokemon list from a grid
print("RE-extracting a pokemon list from a grid\n")
pokemonList = extractPokemonGrid(driver)
print(pokemonList)

#Perform a search in the search field bar
print("Using the search bar\n")
search_bar = driver.find_element(By.CLASS_NAME, 'search-field')
search_bar.clear()
search_bar.send_keys("squirtle")
search_bar.send_keys(Keys.RETURN)
print(driver.current_url)
time.sleep(1)

#click a button
print("Using a button\n")
button = driver.find_element(By.NAME,'add-to-cart')
button.click()

time.sleep(1)
driver.close()

