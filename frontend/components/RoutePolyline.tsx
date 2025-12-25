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
    
    // デバッグログ追加：デコードされたパスの座標を確認
    console.log("Encoded Geometry:", encodedGeometry);
    console.log("Decoded Path Length:", path.length);
    if (path.length > 0) {
        console.log("Decoded Path First Point:", path[0].toJSON());
        console.log("Decoded Path Last Point:", path[path.length-1].toJSON());
    }

    const newPolyline = new google.maps.Polyline({
      path,
      geodesic: true,
      strokeColor: '#0000FF', // 青色に変更して視認性を高める
      strokeOpacity: 0.8,
      strokeWeight: 6,
    });

    newPolyline.setMap(map);
    polylineRef.current = newPolyline;

    // ルート全体が見えるようにズーム調整
    const bounds = new google.maps.LatLngBounds();
    path.forEach((point) => bounds.extend(point));
    
    // 境界が有効か確認してからfitBoundsを実行
    if (!bounds.isEmpty()) {
        console.log("Fitting bounds to:", bounds.toJSON());
        map.fitBounds(bounds);
    } else {
        console.warn("Bounds are empty, skipping fitBounds");
    }

    return () => {
      if (newPolyline) {
        newPolyline.setMap(null);
      }
    };
  }, [map, geometryLibrary, encodedGeometry]);

  return null;
};
