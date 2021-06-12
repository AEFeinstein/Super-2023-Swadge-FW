/*
 * bresenham.h
 *
 *  Created on: Mar 3, 2019
 *      Author: Adam Feinstein
 */

#ifndef SRC_BRESENHAM_H_
#define SRC_BRESENHAM_H_
#include "ssd1306.h"
void plotLine(int x0, int y0, int x1, int y1, color col);
void plotRect(int x0, int y0, int x1, int y1, color col);
void plotEllipse(int xm, int ym, int a, int b, color col);
void plotOptimizedEllipse(int xm, int ym, int a, int b, color col);
void plotCircle(int xm, int ym, int r, color col);
void plotEllipseRect(int x0, int y0, int x1, int y1, color col);
void plotQuadBezierSeg(int x0, int y0, int x1, int y1, int x2, int y2, color col);
void plotQuadBezier(int x0, int y0, int x1, int y1, int x2, int y2, color col);
void plotQuadRationalBezierSeg(int x0, int y0, int x1, int y1, int x2, int y2, float w, color col);
void plotQuadRationalBezier(int x0, int y0, int x1, int y1, int x2, int y2, float w, color col);
void plotRotatedEllipse(int x, int y, int a, int b, float angle, color col);
void plotRotatedEllipseRect(int x0, int y0, int x1, int y1, long zd, color col);
void plotCubicBezierSeg(int x0, int y0, float x1, float y1, float x2, float y2, int x3, int y3, color col);
void plotCubicBezier(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, color col);
void plotQuadSpline(int n, int x[], int y[], color col);
void plotCubicSpline(int n, int x[], int y[], color col);

#endif /* SRC_BRESENHAM_H_ */
