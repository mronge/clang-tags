//
//  PointFilter.h
//  Pilot
//
//  Created by Matt Ronge on 1/25/11.
//  Copyright 2011 Digital Cyclone. All rights reserved.
//

#import <Foundation/Foundation.h>

/**
 * This class is used to lookup point data from the point db's. It
 * will query all of them and find the nearest points to the specified
 * lat/lon
 */
@interface PointLookup : NSObject {
    NSUInteger searchRadius;
    NSUInteger limitPerType;
    BOOL publicOnly;
    BOOL includeHeliports;
    BOOL includeSeaplaneBases;
    BOOL includeUltralights;

    NSArray *cachedPoints;
}
/** Search radius in meters */
@property (nonatomic) NSUInteger searchRadius; 
/** The max number of items to return per point type. Default=10 */
@property (nonatomic) NSUInteger limitPerType; 
/** For airports allow only public ones */
@property (nonatomic) BOOL publicOnly;
@property (nonatomic) BOOL includeHeliports;
@property (nonatomic) BOOL includeSeaplaneBases;
@property (nonatomic) BOOL includeUltralights;
@end
