import { mount } from '@cypress/vue'
import App from '../../src/App.vue'

describe('App Component', () => {
  it('renders the text dynamically', () => {
    mount(App, {
      props: {
        msg: 'Hello Cypress!'
      }
    })
    cy.get('h1').contains('Hello Cypress!')
  })
})
