import pygame
import numpy as np
from dstar import find_path
from tkinter import *
from tkinter import messagebox

# Initializing window using pygame
WIDTH = 800
WINDOW = pygame.display.set_mode((WIDTH, WIDTH))
pygame.display.set_caption("D* Visualization")

ROWS = 40  # Sets how many rows are in the grid/how large the grid is

# Initializing colors
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 255, 0)
YELLOW = (255, 255, 0)
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
PURPLE = (128, 0, 128)
ORANGE = (255, 165, 0)
GREY = (128, 128, 128)
TURQUOISE = (64, 224, 208)


# Creating node class that represents each grid square
class Node:
    def __init__(self, row, col, width, total_rows):
        self.row = row
        self.col = col
        self.x = row * width
        self.y = col * width
        self.color = WHITE
        self.width = width
        self.total_rows = total_rows
        self.value = 0

    def get_position(self):
        return self.row, self.col

    def is_open(self):
        return self.color == WHITE

    def is_barrier(self):
        return self.color == BLACK

    def is_path(self):
        return self.color == ORANGE

    def is_start(self):
        return self.color == TURQUOISE

    def is_end(self):
        return self.color == PURPLE

    def reset(self):
        self.make_open()

    def make_start(self):
        self.color = TURQUOISE

    def make_open(self):
        self.value = 0
        self.color = WHITE

    def make_barrier(self):
        self.value = 1
        self.color = BLACK

    def make_path(self):
        self.color = ORANGE

    def make_end(self):
        self.color = PURPLE

    def draw_node(self, window):
        pygame.draw.rect(window, self.color, pygame.Rect(self.x, self.y, self.width, self.width))


# Making a 2D grid filled with node objects
def make_grid(rows, width):
    grid = np.empty((rows, rows), dtype=Node)
    space = width // rows

    for i in range(rows):
        for j in range(rows):
            node = Node(i, j, space, rows)
            grid[i][j] = node

    return grid


# Drawing the lines on the screen that create the appearance of a grid
def draw_grid(window, rows, width):
    space = width // rows

    for i in range(rows):
        pygame.draw.line(window, GREY, [0, i * space], [width, i * space])
        for j in range(rows):
            pygame.draw.line(window, GREY, [j * space, 0], [j * space, width])


# Drawing the grid and all the nodes onto the screen with their corresponding color
def draw(window, grid, rows, width):
    window.fill(WHITE)

    for row in grid:
        for node in row:
            node.draw_node(window)

    draw_grid(window, rows, width)
    pygame.display.update()


# Getting the position with the grid where the mouse clicked
def get_clicked_pos(pos, rows, width):
    space = width // rows
    y, x = pos

    row = y // space
    col = x // space

    return row, col


def show_error_message(message):
    Tk().wm_withdraw()
    messagebox.showerror(title="Error", message=message)


def main(window, width):
    grid = make_grid(ROWS, width)

    # Both are normally None
    start = grid[0][0]
    end = grid[ROWS - 1][ROWS - 1]

    # Initializing start and end nodes
    start.make_start()
    end.make_end()

    running = True
    lastComputedPath = []
    while running:
        draw(window, grid, ROWS, width)
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
                break

            # Checking if the left mouse button has been pressed
            if pygame.mouse.get_pressed()[0]:
                pos = pygame.mouse.get_pos()
                row, col = get_clicked_pos(pos, ROWS, width)

                if not 0 <= row < ROWS or not 0 <= col < ROWS:
                    continue

                node = grid[row][col]
                if node.is_start() or node.is_end():
                    continue

                # Normally elif
                if node.is_open() or node.is_path():
                    node.make_barrier()
                else:
                    node.make_open()

            # Checking if the right mouse button has been pressed
            elif pygame.mouse.get_pressed()[2]:
                pos = pygame.mouse.get_pos()
                row, col = get_clicked_pos(pos, ROWS, width)

                if not 0 <= row < ROWS or not 0 <= col < ROWS:
                    continue

                node = grid[row][col]
                node.reset()  # Making the node white

            # Checking if a key is pressed
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_g:
                    path = find_path(grid)

                    if len(path) == 0:
                        show_error_message("No possible path")
                        continue

                    for point in lastComputedPath:
                        x, y = point
                        if grid[x][y].is_path():
                            grid[x][y].make_open()

                    for point in path:
                        x, y = point
                        grid[x][y].make_path()
                    lastComputedPath = path
                elif event.key == pygame.K_r:
                    for row in grid:
                        for node in row:
                            node.reset()
                            start.make_start()
                            end.make_end()

    pygame.quit()


main(WINDOW, WIDTH)
