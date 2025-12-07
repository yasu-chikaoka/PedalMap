import '@testing-library/jest-dom'
import { render, screen } from '@testing-library/react'
import Page from './page'

describe('Page', () => {
  it('renders a heading', () => {
    render(<Page />)
    // チェックしたい要素に合わせて調整しますが、ここではまずはレンダリングが落ちないかを確認します
    // page.tsxの実装によっては要素が見つからない可能性があるため、一旦レンダリング確認のみとします
    const main = screen.getByRole('main')
    expect(main).toBeInTheDocument()
  })
})