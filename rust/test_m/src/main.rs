fn main() {
    println!("Hello, world!");
}

#[derive(Default, psi_macros::Fracpack)]
pub struct EmbeddedStruct {
    pub embeded_field_a: u64,
    pub embeded_field_c: f64,
}

#[derive(Default, psi_macros::Fracpack)]
pub struct OuterStruct {
    pub field_a: u64,
    pub field_b: EmbeddedStruct,
    pub field_c: f64,
}
