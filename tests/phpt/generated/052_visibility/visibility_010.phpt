--TEST--
private and protected member access vector 010
--FILE--
<?php
class GeneratedVisibility010 {
    private $left = 74; protected $right = 56;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility010())->total();
--EXPECT--
130
