import { useEffect, useRef } from 'react';
import { useMap, useMapsLibrary } from '@vis.gl/react-google-maps';

interface RoutePolylineProps {
  encodedGeometry: string;
}

export const RoutePolyline = ({ encodedGeometry }: RoutePolylineProps) => {
  const map = useMap();
  const geometryLibrary = useMapsLibrary('geometry');
  const polylineRef = useRef<google.maps.Polyline | null>(null);

  useEffect(() => {
    if (!map || !geometryLibrary) return;

    // 既存のPolylineがあれば削除
    if (polylineRef.current) {
      polylineRef.current.setMap(null);
    }

    const path = geometryLibrary.encoding.decodePath(encodedGeometry);

    const newPolyline = new google.maps.Polyline({
      path,
      geodesic: true,
      strokeColor: '#0000FF',
      strokeOpacity: 0.8,
      strokeWeight: 6,
    });

    newPolyline.setMap(map);
    polylineRef.current = newPolyline;

    return () => {
      if (newPolyline) {
        newPolyline.setMap(null);
      }
    };
  }, [map, geometryLibrary, encodedGeometry]);

  return null;
};
