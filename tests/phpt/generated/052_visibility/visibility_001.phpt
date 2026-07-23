--TEST--
private and protected member access vector 001
--FILE--
<?php
class GeneratedVisibility001 {
    private $left = 11; protected $right = 11;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility001())->total();
--EXPECT--
22
