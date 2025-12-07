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
      strokeColor: '#FF0000',
      strokeOpacity: 1.0,
      strokeWeight: 4,
    });

    newPolyline.setMap(map);
    polylineRef.current = newPolyline;

    // ルート全体が見えるようにズーム調整
    const bounds = new google.maps.LatLngBounds();
    path.forEach((point) => bounds.extend(point));
    map.fitBounds(bounds);

    return () => {
      if (newPolyline) {
        newPolyline.setMap(null);
      }
    };
  }, [map, geometryLibrary, encodedGeometry]);

  return null;
};
