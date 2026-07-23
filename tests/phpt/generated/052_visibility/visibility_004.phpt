--TEST--
private and protected member access vector 004
--FILE--
<?php
class GeneratedVisibility004 {
    private $left = 32; protected $right = 26;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility004())->total();
--EXPECT--
58
