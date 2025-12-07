import { render, screen } from '@testing-library/react'
import Home from './page'
 
// モック: Google Maps API関連のコンポーネント
jest.mock('@vis.gl/react-google-maps', () => ({
  APIProvider: ({ children }: { children: React.ReactNode }) => <div>{children}</div>,
  Map: ({ children }: { children: React.ReactNode }) => <div>Mock Map {children}</div>,
  useMapsLibrary: () => null,
  useMap: () => null,
}))

// モック: PlaceAutocomplete
jest.mock('@/components/PlaceAutocomplete', () => ({
  PlaceAutocomplete: () => <input data-testid="mock-autocomplete" />,
}))

// モック: RoutePolyline
jest.mock('@/components/RoutePolyline', () => ({
  RoutePolyline: () => <div data-testid="mock-polyline" />,
}))
 
describe('Home', () => {
  it('renders the heading', () => {
    render(<Home />)
 
    const heading = screen.getByRole('heading', { level: 1 })
 
    expect(heading).toBeInTheDocument()
    expect(heading).toHaveTextContent('Cycling Route Generator')
  })
})